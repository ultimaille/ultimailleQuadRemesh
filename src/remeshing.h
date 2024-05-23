#include <cmath>
#include <iostream>
#include <iterator>
#include <list>
#include <ostream>
#include <ultimaille/all.h>
#include <unistd.h>
#include <vector>
#include "ultimaille/algebra/vec.h"
#include "matrixEquations.h"
#include "ultimaille/attributes.h"
#include "ultimaille/surface.h"
#include "bvh.h"
#include <assert.h>


using namespace UM;
using Halfedge = typename Surface::Halfedge;
using Facet = typename Surface::Facet;
using Vertex = typename Surface::Vertex;

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Remeshing

inline vec3 getBarycentre(Quads& m, std::vector<int>& edgesAndDefectPointsOnBoundary){
    int n = edgesAndDefectPointsOnBoundary.size();
    vec3 barycentre = vec3(0,0,0);
    for (int i=0; i<n; i++)
        barycentre += Vertex(m, edgesAndDefectPointsOnBoundary[i]).pos();
    return barycentre/n;
}

inline int pyMod(int a, int b){
    return (a%b+b)%b;
}

inline void cleaningTopology(Quads& m, FacetAttribute<int>& fa){
    for (int i=0; i < m.nfacets(); i++){
        if (fa[i] > 0){
            m.conn.get()->active[i] = false;
        }
    }
    //std::cout << "NOCOMPACT" << std::endl;
    m.compact(true); 
}

inline Triangles quand2tri(Quads& m){
    Triangles m2;
    m2.points.create_points(m.nverts());
    for(Vertex v : m.iter_vertices()){
        m2.points[v]= v.pos();
    }
    m2.create_facets(m.nfacets()*2);
    for (auto f: m.iter_facets()){
        m2.vert(2*f, 0) = m.vert(f, 0);
        m2.vert(2*f, 1) = m.vert(f, 1);
        m2.vert(2*f, 2) = m.vert(f, 2);

        m2.vert(2*f+1, 0) = m.vert(f, 0);
        m2.vert(2*f+1, 1) = m.vert(f, 2);
        m2.vert(2*f+1, 2) = m.vert(f, 3);
    }
    return m2;
}

inline void meshingQuad(std::vector<int>& anodes, std::vector<int>& bnodes, std::vector<int>& cnodes, std::vector<int>& dnodes, Quads& m, BVH bvh, int v){
    // assert that we have a topological rectangle
    assert(anodes.size() == cnodes.size());
    assert(bnodes.size() == dnodes.size());

    // assert that edges of the rectangle are correctly connected and oriented
    //        b ->
    //      - - - - -
    //   ^ | + + + + | ^
    // a | | + + + + | | c
    //     | + + + + |  
    //     - - - - - -
    //       d ->
    assert(anodes[anodes.size()-1] == bnodes[0]);
    assert(bnodes[bnodes.size()-1] == cnodes[cnodes.size()-1]);
    assert(dnodes[dnodes.size()-1] == cnodes[0]);
    assert(anodes[0] == dnodes[0]);


    if (v==35)
        std::cout << "reached" << std::endl;
        

    if (anodes.size() < 3 && bnodes.size() < 3){
        m.conn->create_facet({anodes[0], bnodes[0], bnodes[1], cnodes[0]});
        return;
    }

    //assert(anodes.size()>2 || bnodes.size()>2);

    bool reversed = false;
    if (anodes.size()<3){
        std::swap(anodes, dnodes);
        std::swap(cnodes, bnodes);
        reversed = true;
    }
    int a = anodes.size();
    int b = bnodes.size();

    m.points.create_points((a-2)*(b-2));

    for (int i=1; i<a; i++){
        for (int j=1; j<b; j++){
            //  - - - - -
            // | X X X X |
            // | X X X X |
            // | X X X X |
            // - - - - - -

            // we'll construct the points starting from the bottom left and decrementing starting from m.nverts()
            // TODO: consider the 4 points on the grid instead of just 2
            int newPointIndex = 0;
            int btmNewPointIndex = 0;
            if (b<3){
                newPointIndex = cnodes[i];
                btmNewPointIndex = cnodes[i-1];
            }
            else{
                vec3 x0 = Vertex(m, anodes[i]).pos();
                vec3 x1 = Vertex(m, cnodes[i]).pos();

                vec3 newPoint = x0 + j*(x1-x0)/ (b-1); 

                newPointIndex = m.nverts()-  ((i-1)*(b-2) + j);
                btmNewPointIndex = newPointIndex + (b-2); 
                if (i<a-1 && j<b-1)
                    m.points[newPointIndex] = bvh.project(newPoint);
            }

            // TODO: find a more elegant way of dealing with reversed facets?
            if (reversed){
                if (i==1 && j==1)
                    m.conn->create_facet({dnodes[0], dnodes[1], newPointIndex, anodes[1]});
                else if (i==1 && j<b-1)
                    m.conn->create_facet({dnodes[j-1], dnodes[j], newPointIndex, newPointIndex+1});
                else if (i==1 && j==b-1)
                    m.conn->create_facet({dnodes[j-1], dnodes[j], cnodes[i], newPointIndex+1});
                else if (i<a-1 && j==1)
                    m.conn->create_facet({anodes[i-1], btmNewPointIndex, newPointIndex, anodes[i]});
                else if (i<a-1 && j<b-1)
                    m.conn->create_facet({btmNewPointIndex+1, btmNewPointIndex, newPointIndex, newPointIndex+1});
                else if (i<a-1 && j==b-1)
                    m.conn->create_facet({btmNewPointIndex+1, cnodes[i-1], cnodes[i], newPointIndex+1});
                else if (i==a-1 && j==1)
                    m.conn->create_facet({anodes[a-2], btmNewPointIndex, bnodes[1], bnodes[0]});
                else if (i==a-1 && j<b-1)
                    m.conn->create_facet({btmNewPointIndex+1, btmNewPointIndex, bnodes[j], bnodes[j-1]});
                else if (i==a-1 && j==b-1)
                    m.conn->create_facet({btmNewPointIndex+1, cnodes[a-2], cnodes[a-1], bnodes[b-2]});
            } else {
                if (i==1 && j==1)
                    m.conn->create_facet({anodes[1], newPointIndex, dnodes[1], dnodes[0]});
                else if (i==1 && j<b-1)
                    m.conn->create_facet({newPointIndex+1, newPointIndex, dnodes[j], dnodes[j-1]});
                else if (i==1 && j==b-1)
                    m.conn->create_facet({newPointIndex+1, cnodes[i], dnodes[j], dnodes[j-1]});
                else if (i<a-1 && j==1)
                    m.conn->create_facet({anodes[i], newPointIndex, btmNewPointIndex, anodes[i-1]});
                else if (i<a-1 && j<b-1)
                 m.conn->create_facet({newPointIndex+1, newPointIndex, btmNewPointIndex, btmNewPointIndex+1});
                else if (i<a-1 && j==b-1)
                  m.conn->create_facet({newPointIndex+1, cnodes[i], cnodes[i-1], btmNewPointIndex+1});
                else if (i==a-1 && j==1)
                   m.conn->create_facet({bnodes[0], bnodes[1], btmNewPointIndex, anodes[a-2]});
                else if (i==a-1 && j<b-1)
                    m.conn->create_facet({bnodes[j-1], bnodes[j], btmNewPointIndex, btmNewPointIndex+1});
                else if (i==a-1 && j==b-1)
                    m.conn->create_facet({bnodes[b-2], cnodes[a-1], cnodes[a-2], btmNewPointIndex+1});
                }
            }
    }
}


inline void divideInSubPatches2(int* partSegments, std::list<int>& patch, Quads& m, int size, FacetAttribute<int>& fa, BVH bvh, int v){
    // TODO: pass BVH by reference for perfs?
    std::vector<std::vector <int>> anodesList(size);
    std::vector<std::vector <int>> dnodesList(size);

    // a nodes and d nodes
    // performances on array initialization?
        auto it = patch.begin();
        it++;
        for (int i=0;i<size;i++){
            it--;
            for (int j=0;j<partSegments[2*i]+1;j++){
                anodesList[i].push_back(Halfedge(m, *it).from());
                it++;
            }
            it--;
            for (int j=0;j<partSegments[2*i+1]+1;j++){
                dnodesList[i].push_back(Halfedge(m, *it).from());
                it++;
            }
        }
        for (auto& nodeList : dnodesList){
            std::reverse(nodeList.begin(), nodeList.end());
        }
        std::rotate(dnodesList.rbegin(), dnodesList.rbegin() + 1, dnodesList.rend());
        dnodesList[0][0] = anodesList[0][0];

    // Barycentre !!! 
    // TODO: put more things in the function barycentre
        std::vector<int> baryNodes(size, 0);
        for (int i=0;i<size;i++){
            baryNodes[i]=anodesList[i][anodesList[i].size()-1];
        }
        vec3 barycentrePos = bvh.project(getBarycentre(m, baryNodes));
        m.points.create_points(1);
        int barycentreIndex = m.nverts()-1;
        m.points[barycentreIndex]=barycentrePos;


    // b nodes
        std::vector<std::vector <int>> bnodesList(size);

        for (int i=0;i<size;i++){
            int x0index = anodesList[i][anodesList[i].size()-1];
            bnodesList[i].push_back(x0index);
            vec3 x0 = Vertex(m, x0index).pos();
            vec3 x1 = barycentrePos;
            int bnodesLen = (int)dnodesList[i].size();
            m.points.create_points(bnodesLen-1);
            for (int j=1;j<bnodesLen-1;j++){
                // Make the new point
                vec3 newPoint = x0 +j*(x1-x0) / (bnodesLen-1);
                int newPointIndex = m.nverts()-j;
                m.points[newPointIndex] = bvh.project(newPoint);
                bnodesList[i].push_back(newPointIndex);
            }
            bnodesList[i].push_back(barycentreIndex);
        } 

    // c nodes
        std::vector<std::vector <int>> cnodesList = bnodesList;
        std::rotate(cnodesList.rbegin(), cnodesList.rbegin() + 1, cnodesList.rend());


    // Do the magik 
    for (int i=0; i<size; i++){
        meshingQuad(anodesList[i], bnodesList[i], cnodesList[i], dnodesList[i], m, bvh, v);
    } 

/*    if (v==35){
        for (int i=0; i < m.nfacets(); i++){
            if (fa[i] > 0){
                m.conn.get()->active[i] = false;
            }
        }
        //std::cout << "NOCOMPACT" << std::endl;
        m.compact(false); 
        write_by_extension("outputH.geogram", m, {{}, {{"patch", fa.ptr},},{}});
    } */







    //exit(0);


}

inline void segmentConstruction(std::list<int>& patchConvexity, int* segments, int edge){
    // construct array with the number of points between each edge
    int n = edge;
    int edgeLength = 0;
    int skip = 1;
    for (int convexity : patchConvexity){
        if (skip){
            skip = 0;
            continue;
        } 
        edgeLength++;
        if (convexity >= 1){
            segments[n-edge] = edgeLength;
            edge--;
            edgeLength = 0;
        }
    }
    segments[n-1] = edgeLength+1;
}

inline void fillingConvexPos(std::list<int>& patchConvexity, std::vector<int>& convexPos){
    int count = 0;
    int letterToFill = 0;
    for (int v : patchConvexity){
        if (v >= 1){
            convexPos[letterToFill]=count;
            letterToFill++;
        }
        count++;
    }
}



inline bool testRotations(std::vector <int>& convexPos, std::vector<int>& cumulConvexity, int& rotation, int size){
    rotation = 0;
    for (int i=0; i<(int)convexPos.size();i++){
        rotation = convexPos[i];
        if (pyMod(convexPos[i]-rotation,size)==cumulConvexity[0]
            && pyMod(convexPos[(i+1)%4]-rotation,size)==cumulConvexity[1]
            && pyMod(convexPos[(i+2)%4]-rotation,size)==cumulConvexity[2]
            && pyMod(convexPos[(i+3)%4]-rotation,size)==cumulConvexity[3])
        {
            return true;
        }
    }
    return false;
}

inline void rotateToA(std::list<int>& patch, std::list<int>& patchConvexity, int a, int b, int c){
    std::vector<int> convexPos = {0,0,0,0};
    std::vector<int> cumulConvexity = {0,0,0,0};
    cumulConvexity[1] = a;
    cumulConvexity[2] = a+b;
    cumulConvexity[3] = a+b+c; 

    // Filling convex pos
    fillingConvexPos(patchConvexity, convexPos);
    
    int rotation = 0;
    int size = patch.size();
    bool found = testRotations(convexPos, cumulConvexity, rotation, size);

    if (!found){
        patch.reverse();
        patchConvexity.reverse();
        fillingConvexPos(patchConvexity, convexPos);
        found = testRotations(convexPos, cumulConvexity, rotation, size);
    }
    assert(found);

    // rotating the patch
    for (int i=0; i<rotation; i++){
        patch.push_back(patch.front());
        patch.pop_front();
        patchConvexity.push_back(patchConvexity.front());
        patchConvexity.pop_front();
    }
}


inline int roundUpDivide(int a, int b){
    return (a+b-1)/b;
}

inline bool find(std::list<int>& v, int x){
    return std::find(v.begin(), v.end(), x) != v.end();
}

inline int solve4equations(int* segments, int* partSegments){
    // TODO: return string instead of int
    // TODO: put in other file
    if (segments[0] == segments[2] && segments[1] == segments[3]){
        std::cout << "PERFECT QUAD REMESH POSSIBLE" << std::endl;
        return 4;
    }
    int a = fmax(segments[0], segments[2]);
    int c = fmin(segments[0], segments[2]);
    int b = fmin(segments[1], segments[3]);
    int d = fmax(segments[1], segments[3]);

    int segmentsTri[] = {d-b,  c, a};
    if (solve3equations(segmentsTri, partSegments)){
        //std::cout << "SOLUTION 1 FOUND !" << std::endl;
        return -4;
    }

    // TODO: Sideway triangle insertion
    //int segmentsTri2[] = {d, b, a-c};
    //if (solve3equations(segmentsTri2, partSegments)){
    //    std::cout << "SOLUTION 2 FOUND !" << std::endl;
    //    return false;
    //}


    return -1;
}

inline void patchToNodes(std::list<int>& patch, int* segments, std::vector<int>& anodes, std::vector<int>& bnodes, std::vector<int>& cnodes, std::vector<int>& dnodes, Quads& m){
    int aSize = segments[1]+1;
    int bSize = segments[0]+1;

    auto it = patch.begin();   
    for (int i = 0; i < (int) patch.size(); i++){

        if (i==0){
            anodes[aSize-1] = Halfedge(m, *it).from();
            bnodes[0] = Halfedge(m, *it).from();
        } else if (i < segments[0]){
            bnodes[i] = Halfedge(m, *it).from();
        } else if (i == segments[0]){
            assert(i==bSize-1);
            bnodes[bSize-1] = Halfedge(m, *it).from();
            cnodes[aSize-1] = Halfedge(m, *it).from();
        } else if (i < segments[0]+segments[1]){
            cnodes[aSize-1-i+segments[0]] = Halfedge(m, *it).from();
        } else if (i == segments[0]+segments[1]){
            cnodes[0] = Halfedge(m, *it).from();
            dnodes[bSize-1] = Halfedge(m, *it).from();
        } else if (i < segments[0]+segments[1]+segments[2]){
            dnodes[bSize-1-i+segments[0]+segments[1]] = Halfedge(m, *it).from();
        } else if (i == segments[0]+segments[1]+segments[2]){
            dnodes[0] = Halfedge(m, *it).from();
            anodes[0] = Halfedge(m, *it).from();
        } else {
            anodes[i-segments[0]-segments[1]-segments[2]] = Halfedge(m, *it).from();
        }
        it++;
    }
    std::cout << "anodes: ";
    for (int i : anodes){
        std::cout << i << " ";
    }
    std::cout << std::endl;
    std::cout << "bnodes: ";
    for (int i : bnodes){
        std::cout << i << " ";
    }
    std::cout << std::endl;
    std::cout << "cnodes: ";
    for (int i : cnodes){
        std::cout << i << " ";
    }
    std::cout << std::endl;
    std::cout << "dnodes: ";
    for (int i : dnodes){
        std::cout << i << " ";
    }
    std::cout << std::endl;
}

inline void createPointsBetween2Vx(std::vector<int>& nodes, int n, Quads& m){
    m.points.create_points(n-1);
    for (int i=1; i<n; i++){
        vec3 x0 = Vertex(m, nodes[0]).pos();
        vec3 x1 = Vertex(m, nodes[n]).pos();
        vec3 newPoint = x0 + i*(x1-x0)/n;
        nodes[i] = m.nverts()-i;
        m.points[m.nverts()-i] = newPoint;
    }
}

inline bool remeshingPatch(std::list<int>& patch, std::list<int>& patchConvexity, int nEdge, Quads& m, FacetAttribute<int>& fa, int v, BVH bvh){
    assert(patchConvexity.front() >= 1);
    assert(nEdge == 3 || nEdge == 5 || nEdge == 4);

    int segments[] = {0,0,0,0,0};
    int partSegments[] = {0,0,0,0,0,0,0,0,0,0};
    segmentConstruction(patchConvexity, segments, nEdge);


    if (nEdge == 4){
        //print segments
        //std::cout << "Segments: " << segments[0] << " " << segments[1] << " " << segments[2] << " " << segments[3] << std::endl;
        nEdge = solve4equations(segments, partSegments);
        if (nEdge == -1){
            return false;
        }
    }
    else if (nEdge == 3){
        if (!solve3equations(segments, partSegments)){
            return false;
        }
    }
    else if (nEdge == 5){
        if (!solve5equations(segments, partSegments)){
            return false;
        }
    }


    // patch into triangles for projection
    // first we're gonna have to create a new mesh with just the patch
    // TODO: Put that in a function
    Quads mPatch;
    std::vector<int> facetsInPatch;
    for (int i = 0; i < m.nfacets(); i++){
        if (fa[i] > 0){
            facetsInPatch.push_back(i);
        }
    }
    // Deep copying mesh 
    mPatch.points.create_points(m.nverts());
    for (Vertex v : m.iter_vertices()){
        mPatch.points[v] = v.pos();
    }
    mPatch.create_facets(facetsInPatch.size());
    for (int i = 0; i < (int) facetsInPatch.size(); i++){
        Facet f = Facet(m, facetsInPatch[i]);
        mPatch.vert(i, 0) = m.vert(f, 0);
        mPatch.vert(i, 1) = m.vert(f, 1);
        mPatch.vert(i, 2) = m.vert(f, 2);
        mPatch.vert(i, 3) = m.vert(f, 3);
    }
    m.compact(true);
    Triangles mTri = quand2tri(mPatch);
    BVH bvhPatch(mTri);


    std::cout << "solve" << nEdge << "equations success, root: " << v << std::endl;


/* 
    if (nEdge == -4) {
        int a = fmax(segments[0], segments[2]);
        int c = fmin(segments[0], segments[2]);
        int b = fmin(segments[1], segments[3]);
        int d = fmax(segments[1], segments[3]);

        // First we'll rotate the patch so the patch starts at beginning of a followed by b
        rotateToA(patch, patchConvexity, a, b, c);
        write_by_extension("outputA.geogram", m, {{}, {{"patch", fa.ptr}, }, {}});
        
        a++;b++;c++;d++; // because we want to include the last points
        // Then we'll construct the left regular quad
        std::vector<int> anodes(a, 0);
        auto it = patch.begin();
        for (int i=0; i<a; i++){
            anodes[i] = Halfedge(m, *it).from();
            it++;
        }

        // b nodes goes from the last node of a (included) to half of b
        std::vector<int> bnodes(roundUpDivide(b, 2), 0);
        it--;
        for (int i=0; i<roundUpDivide(b, 2); i++){
            bnodes[i] = Halfedge(m, *it).from();
            it++;
        }

        // c nodes must be constructed
        std::vector<int> cnodes(a, 0);
        it --;
        Halfedge h = Halfedge(m, *it);
        cnodes[0] = h.from();
        it = patch.end();
        std::advance(it, -b/2);
        cnodes[a-1] = Halfedge(m, *it).from();

        // we construct a-2 points distributed between cnodes[0] and cnodes[a-1] and put them in cnodes
        createPointsBetween2Vx(cnodes, a-1, m);
        std::reverse(cnodes.begin(), cnodes.end());


        write_by_extension("outputB.geogram", m, {{}, {{"patch", fa.ptr}, }, {}});

        // Let's do d nodes now
        std::vector<int> dnodes(roundUpDivide(b, 2), 0);
        dnodes[0]=Halfedge(m, *patch.begin()).from();
        it = patch.end();
        it--;
        for (int i=1; i<roundUpDivide(b, 2); i++){
            dnodes[i] = Halfedge(m, *it).from();
            it--;
        }

        meshingQuad(anodes, bnodes, cnodes, dnodes, m, bvhPatch);



        // Let's work on the second rectangle, starting from a nodes. Its a node has to be constructed
        std::vector<int> anodes2(c, 0);
        anodes2[0] = bnodes[b-2];
        it = patch.begin();
        std::advance(it, a+b+c-3+(b-1)/2);
        anodes2[c-1] = Halfedge(m, *it).from();

        createPointsBetween2Vx(anodes2, c-1, m); // TODO: verify
        std::reverse(anodes2.begin(), anodes2.end());

        // b nodes
        // TODO: attention parité
        std::vector bnodes2(roundUpDivide(b,2), 0);
        it = patch.begin();
        std::advance(it, a+b-2 - roundUpDivide(b,2)+1);
        for (int i=0; i<(int)bnodes2.size(); i++){
            bnodes2[i] = Halfedge(m, *it).from();
            it++;
        }

        // c nodes
        std::vector<int> cnodes2(c, 0);
        it--;
        for (int i=0; i<c; i++){
            cnodes2[i] = Halfedge(m, *it).from();
            it++;
        }
        std::reverse(cnodes2.begin(), cnodes2.end());

        // d nodes
        std::vector<int> dnodes2(bnodes.size(), 0);
        it--;
        for (int i=0; i<(int)dnodes2.size(); i++){
            dnodes2[i] = Halfedge(m, *it).from();
            it++;
        }
        std::reverse(dnodes2.begin(), dnodes2.end());

        meshingQuad(anodes2, bnodes2, cnodes2, dnodes2, m, bvhPatch);



       

        // Let's tackle the 3 patch now 
        
        // we just need the bottom part of the patch first
        std::vector<int> btmPart(d-b+1, 0);
        it--;
        for (int i=0; i<(int)btmPart.size(); i++){
            btmPart[i] = Halfedge(m, *it).from();
            it++;
        }


        std::list<int> lst;
        std::reverse(anodes2.begin(), anodes2.end());

        lst.insert(lst.end(), anodes2.begin(), anodes2.end());
        lst.insert(lst.end(), btmPart.begin(), btmPart.end());
        lst.insert(lst.end(), cnodes.begin(), cnodes.end());


        cleaningTopology(m, fa);
        write_by_extension("outputD.geogram", m, {{}, {{"patch", fa.ptr}, }, {}});
        exit(0);



 */



    //} else 
    if (nEdge == 3 || nEdge == 5){
        divideInSubPatches2(partSegments, patch, m, nEdge, fa, bvh, v);

    } else if (nEdge == 4){
        
        for (auto i : patch){
            std::cout << Halfedge(m, i).from() << " ";
        }
        std::cout << std::endl;


        // Constructiong the sides
        int aSize = segments[1]+1;
        int bSize = segments[0]+1;
        std::vector<int> anodes(aSize);
        std::vector<int> bnodes(bSize);
        std::vector<int> cnodes(aSize);
        std::vector<int> dnodes(bSize);
        
        patchToNodes(patch, segments, anodes, bnodes, cnodes, dnodes, m);
        meshingQuad(anodes, bnodes, cnodes, dnodes, m, bvhPatch, 0);

    } 
    


    // remove old facets and points (caution: it changes m topology)
    cleaningTopology(m, fa);



    return true;
}
