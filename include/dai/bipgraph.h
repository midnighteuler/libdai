/*  Copyright (C) 2006-2008  Joris Mooij  [joris dot mooij at tuebingen dot mpg dot de]
    Radboud University Nijmegen, The Netherlands /
    Max Planck Institute for Biological Cybernetics, Germany

    This file is part of libDAI.

    libDAI is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    libDAI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libDAI; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


/// \file
/// \brief Defines BipartiteGraph class


#ifndef __defined_libdai_bipgraph_h
#define __defined_libdai_bipgraph_h


#include <ostream>
#include <vector>
#include <cassert>
#include <algorithm>
#include <dai/util.h>


namespace dai {


/// Represents the neighborhood structure of nodes in a bipartite graph.
/** A bipartite graph has two types of nodes: type 1 and type 2. Edges can occur only between
 *  nodes of different type. Nodes are indexed by an unsigned integer. If there are nr1()
 *  nodes of type 1 and nr2() nodes of type 2, the nodes of type 1 are numbered 
 *  0,1,2,...,nr1()-1 and the nodes of type 2 are numbered 0,1,2,...,nr2()-1. An edge
 *  between node \a n1 of type 1 and node \a n2 of type 2 is represented by a BipartiteGraph::Edge(\a n1,\a n2).
 *
 *  A BipartiteGraph is implemented as a sparse adjacency list, i.e., it stores for each node a list of
 *  its neighboring nodes. In particular, it stores for each node of type 1 a vector of Neighbor structures
 *  (accessible by the nb1() method) describing the neighboring nodes of type 2; similarly, for each node 
 *  of type 2 it stores a vector of Neighbor structures (accessibly by the nb2() method) describing the 
 *  neighboring nodes of type 1. 
 *  Thus, each node has an associated variable of type BipartiteGraph::Neighbors, which is a vector of
 *  Neighbor structures, describing its neighboring nodes of the other type.
 *  \idea Cache second-order neighborhoods in BipartiteGraph.
 */
class BipartiteGraph {
    public:
        /// Describes the neighbor relationship of two nodes in a BipartiteGraph.
        /** Sometimes we want to do an action, such as sending a
         *  message, for all edges in a graph. However, most graphs
         *  will be sparse, so we need some way of storing a set of
         *  the neighbors of a node, which is both fast and
         *  memory-efficient. We also need to be able to go between
         *  viewing node \a A as a neighbor of node \a B, and node \a
         *  B as a neighbor of node \a A. The Neighbor struct solves
         *  both of these problems. Each node has a list of neighbors,
         *  stored as a vector<Neighbor>, and extra information is
         *  included in the Neighbor struct which allows us to access
         *  a node as a neighbor of its neighbor (the \a dual member).
         *
         *  By convention, variable identifiers naming indices into a
         *  vector of neighbors are prefixed with an underscore ("_"). 
         *  The neighbor list which they point into is then understood
         *  from context. For example:
         *
         *  \code
         *  void BP::calcNewMessage( size_t i, size_t _I )
         *  \endcode
         *
         *  Here, \a i is the "absolute" index of node i, but \a _I is
         *  understood as a "relative" index, giving node I's entry in
         *  nb1(i). The corresponding Neighbor structure can be
         *  accessed as nb1(i,_I) or nb1(i)[_I]. The absolute index of
         *  \a _I, which would be called \a I, can be recovered from
         *  the \a node member: nb1(i,_I).node. The \a iter member
         *  gives the relative index \a _I, and the \a dual member
         *  gives the "dual" relative index, i.e. the index of \a i in
         *  \a I's neighbor list.
         *
         *  \code
         *  Neighbor n = nb1(i,_I);
         *  n.node == I &&
         *  n.iter == _I &&
         *  nb2(n.node,n.dual).node == i
         *  \endcode
         *
         *  In a FactorGraph, nb1 is called nbV, and nb2 is called
         *  nbF.
         *
         *  There is no easy way to transform a pair of absolute node
         *  indices \a i and \a I into a Neighbor structure relative
         *  to one of the nodes. Such a feature has never yet been
         *  found to be necessary. Iteration over edges can always be
         *  accomplished using the Neighbor lists, and by writing
         *  functions that accept relative indices:
         *  \code
         *  for( size_t i = 0; i < nrVars(); ++i )
         *      foreach( const Neighbor &I, nbV(i) )
         *          calcNewMessage( i, I.iter );
         *  \endcode
         */
        struct Neighbor {
            /// Corresponds to the index of this Neighbor entry in the vector of neighbors
            size_t iter;
            /// Contains the number of the neighboring node
            size_t node;
            /// Contains the "dual" iter
            size_t dual;

            /// Default constructor
            Neighbor() {}
            /// Constructor that sets the Neighbor members according to the parameters
            Neighbor( size_t iter, size_t node, size_t dual ) : iter(iter), node(node), dual(dual) {}

            /// Cast to size_t returns node member
            operator size_t () const { return node; }
        };

        /// Describes the neighbors of some node.
        typedef std::vector<Neighbor> Neighbors;

        /// Represents an edge: an Edge(\a n1,\a n2) corresponds to the edge between node \a n1 of type 1 and node \a n2 of type 2.
        typedef std::pair<size_t,size_t> Edge;

    private:
        /// Contains for each node of type 1 a vector of its neighbors
        std::vector<Neighbors> _nb1;

        /// Contains for each node of type 2 a vector of its neighbors
        std::vector<Neighbors> _nb2;

        /// Used internally by isTree()
        struct levelType {
            std::vector<size_t> ind1;       // indices of nodes of type 1
            std::vector<size_t> ind2;       // indices of nodes of type 2
        };

        // OBSOLETE
        /// @name Backwards compatibility layer (to be removed soon)
        //@{
        /// Enable backwards compatibility layer?
        bool _edge_indexed;
        /// Call indexEdges() first to initialize these members
        std::vector<Edge> _edges;
        /// Call indexEdges() first to initialize these members
        hash_map<Edge,size_t> _vv2e;
        //}@

    public:
        /// Default constructor (creates an empty bipartite graph)
        BipartiteGraph() : _nb1(), _nb2(), _edge_indexed(false) {}

        /// Constructs BipartiteGraph from a range of edges.
        /** \tparam EdgeInputIterator Iterator that iterates over instances of BipartiteGraph::Edge.
         *  \param nr1 The number of nodes of type 1.
         *  \param nr2 The number of nodes of type 2. 
         *  \param begin Points to the first edge.
         *  \param end Points just beyond the last edge.
         */
        template<typename EdgeInputIterator>
        BipartiteGraph( size_t nr1, size_t nr2, EdgeInputIterator begin, EdgeInputIterator end ) : _nb1( nr1 ), _nb2( nr2 ), _edge_indexed(false) {
            construct( nr1, nr2, begin, end );
        }

        /// (Re)constructs BipartiteGraph from a range of edges.
        /** \tparam EdgeInputIterator Iterator that iterates over instances of BipartiteGraph::Edge.
         *  \param nr1 The number of nodes of type 1.
         *  \param nr2 The number of nodes of type 2. 
         *  \param begin Points to the first edge.
         *  \param end Points just beyond the last edge.
         */
        template<typename EdgeInputIterator>
        void construct( size_t nr1, size_t nr2, EdgeInputIterator begin, EdgeInputIterator end );

        /// Returns constant reference to the _i2'th neighbor of node i1 of type 1
        const Neighbor & nb1( size_t i1, size_t _i2 ) const { 
#ifdef DAI_DEBUG
            assert( i1 < _nb1.size() );
            assert( _i2 < _nb1[i1].size() );
#endif
            return _nb1[i1][_i2]; 
        }
        /// Returns reference to the _i2'th neighbor of node i1 of type 1
        Neighbor & nb1( size_t i1, size_t _i2 ) {
#ifdef DAI_DEBUG
            assert( i1 < _nb1.size() );
            assert( _i2 < _nb1[i1].size() );
#endif
            return _nb1[i1][_i2]; 
        }

        /// Returns constant reference to the _i1'th neighbor of node i2 of type 2
        const Neighbor & nb2( size_t i2, size_t _i1 ) const { 
#ifdef DAI_DEBUG
            assert( i2 < _nb2.size() );
            assert( _i1 < _nb2[i2].size() );
#endif
            return _nb2[i2][_i1]; 
        }
        /// Returns reference to the _i1'th neighbor of node i2 of type 2
        Neighbor & nb2( size_t i2, size_t _i1 ) { 
#ifdef DAI_DEBUG
            assert( i2 < _nb2.size() );
            assert( _i1 < _nb2[i2].size() );
#endif
            return _nb2[i2][_i1]; 
        }

        /// Returns constant reference to all neighbors of node i1 of type 1
        const Neighbors & nb1( size_t i1 ) const { 
#ifdef DAI_DEBUG
            assert( i1 < _nb1.size() );
#endif
            return _nb1[i1]; 
        }
        /// Returns reference to all neighbors of node of i1 type 1
        Neighbors & nb1( size_t i1 ) { 
#ifdef DAI_DEBUG
            assert( i1 < _nb1.size() );
#endif
            return _nb1[i1]; 
        }

        /// Returns constant reference to all neighbors of node i2 of type 2
        const Neighbors & nb2( size_t i2 ) const { 
#ifdef DAI_DEBUG
            assert( i2 < _nb2.size() );
#endif
            return _nb2[i2]; 
        }
        /// Returns reference to all neighbors of node i2 of type 2
        Neighbors & nb2( size_t i2 ) { 
#ifdef DAI_DEBUG
            assert( i2 < _nb2.size() );
#endif
            return _nb2[i2]; 
        }

        /// Returns number of nodes of type 1
        size_t nr1() const { return _nb1.size(); }
        /// Returns number of nodes of type 2
        size_t nr2() const { return _nb2.size(); }
        
        /// Calculates the number of edges, time complexity: O(nr1())
        size_t nrEdges() const {
            size_t sum = 0;
            for( size_t i1 = 0; i1 < nr1(); i1++ )
                sum += nb1(i1).size();
            return sum;
        }
        
        /// Adds a node of type 1 without neighbors.
        void add1() { _nb1.push_back( Neighbors() ); }
        
        /// Adds a node of type 2 without neighbors.
        void add2() { _nb2.push_back( Neighbors() ); }

        /// Adds a node of type 1, with neighbors specified by a range of nodes of type 2.
        /** \tparam NodeInputIterator Iterator that iterates over instances of size_t.
         *  \param begin Points to the first index of the nodes of type 2 that should become neighbors of the added node.
         *  \param end Points just beyond the last index of the nodes of type 2 that should become neighbors of the added node.
         *  \param sizeHint For improved efficiency, the size of the range may be specified by sizeHint.
         */
        template <typename NodeInputIterator>
        void add1( NodeInputIterator begin, NodeInputIterator end, size_t sizeHint = 0 ) {
            Neighbors nbs1new;
            nbs1new.reserve( sizeHint );
            size_t iter = 0;
            for( NodeInputIterator it = begin; it != end; ++it ) {
                assert( *it < nr2() );
                Neighbor nb1new( iter, *it, nb2(*it).size() );
                Neighbor nb2new( nb2(*it).size(), nr1(), iter++ );
                nbs1new.push_back( nb1new );
                nb2( *it ).push_back( nb2new );
            }
            _nb1.push_back( nbs1new );
        }

        /// Adds a node of type 2, with neighbors specified by a range of nodes of type 1.
        /** \tparam NodeInputIterator Iterator that iterates over instances of size_t.
         *  \param begin Points to the first index of the nodes of type 1 that should become neighbors of the added node.
         *  \param end Points just beyond the last index of the nodes of type 1 that should become neighbors of the added node.
         *  \param sizeHint For improved efficiency, the size of the range may be specified by sizeHint.
         */
        template <typename NodeInputIterator>
        void add2( NodeInputIterator begin, NodeInputIterator end, size_t sizeHint = 0 ) {
            Neighbors nbs2new;
            nbs2new.reserve( sizeHint );
            size_t iter = 0;
            for( NodeInputIterator it = begin; it != end; ++it ) {
                assert( *it < nr1() );
                Neighbor nb2new( iter, *it, nb1(*it).size() );
                Neighbor nb1new( nb1(*it).size(), nr2(), iter++ );
                nbs2new.push_back( nb2new );
                nb1( *it ).push_back( nb1new );
            }
            _nb2.push_back( nbs2new );
        }

        /// Removes node n1 of type 1 and all incident edges.
        void erase1( size_t n1 );

        /// Removes node n2 of type 2 and all incident edges.
        void erase2( size_t n2 );

        /// Removes edge between node n1 of type 1 and node n2 of type 2.
        void eraseEdge( size_t n1, size_t n2 ) {
            assert( n1 < nr1() );
            assert( n2 < nr2() );
            for( Neighbors::iterator i1 = _nb1[n1].begin(); i1 != _nb1[n1].end(); i1++ )
                if( i1->node == n2 ) {
                    _nb1[n1].erase( i1 );
                    break;
                }
            for( Neighbors::iterator i2 = _nb2[n2].begin(); i2 != _nb2[n2].end(); i2++ )
                if( i2->node == n1 ) {
                    _nb2[n2].erase( i2 );
                    break;
                }
        }

        /// Adds an edge between node n1 of type 1 and node n2 of type 2.
        /** If check == true, only adds the edge if it does not exist already.
         */
        void addEdge( size_t n1, size_t n2, bool check = true ) {
            assert( n1 < nr1() );
            assert( n2 < nr2() );
            bool exists = false;
            if( check ) {
                // Check whether the edge already exists
                foreach( const Neighbor &nb2, nb1(n1) )
                    if( nb2 == n2 ) {
                        exists = true;
                        break;
                    }
            }
            if( !exists ) { // Add edge
                Neighbor nb_1( _nb1[n1].size(), n2, _nb2[n2].size() );
                Neighbor nb_2( nb_1.dual, n1, nb_1.iter );
                _nb1[n1].push_back( nb_1 );
                _nb2[n2].push_back( nb_2 );
            }
        }

        /// Calculates second-order neighbors (i.e., neighbors of neighbors) of node n1 of type 1.
        /** If include == true, includes n1 itself, otherwise excludes n1.
         */
        std::vector<size_t> delta1( size_t n1, bool include = false ) const;

        /// Calculates second-order neighbors (i.e., neighbors of neighbors) of node n2 of type 2.
        /** If include == true, includes n2 itself, otherwise excludes n2.
         */
        std::vector<size_t> delta2( size_t n2, bool include = false ) const;

        /// Returns true if the graph is connected
        /** \todo Should be optimized by invoking boost::graph library
         */
        bool isConnected() const;

        /// Returns true if the graph is a tree, i.e., if it is singly connected and connected.
        bool isTree() const;

        /// Writes this BipartiteGraph to an output stream in GraphViz .dot syntax
        void printDot( std::ostream& os ) const;

        // OBSOLETE
        /// @name Backwards compatibility layer (to be removed soon)
        //@{
        void indexEdges() {
            std::cerr << "Warning: this BipartiteGraph edge interface is obsolete!" << std::endl;
            _edges.clear();
            _vv2e.clear();
            size_t i=0;
            foreach(const Neighbors &nb1s, _nb1) {
                foreach(const Neighbor &n2, nb1s) {
                    Edge e(i, n2.node);
                    _edges.push_back(e);
                }
                i++;
            }
            sort(_edges.begin(), _edges.end()); // unnecessary?

            i=0;
            foreach(const Edge& e, _edges) {
                _vv2e[e] = i++;
            }

            _edge_indexed = true;
        }
        
        const Edge& edge(size_t e) const {
            assert(_edge_indexed);
            return _edges[e];
        }
        
        const std::vector<Edge>& edges() const {
            return _edges;
        }
        
        size_t VV2E(size_t n1, size_t n2) const {
            assert(_edge_indexed);
            Edge e(n1,n2);
            hash_map<Edge,size_t>::const_iterator i = _vv2e.find(e);
            assert(i != _vv2e.end());
            return i->second;
        }

        size_t nr_edges() const {
            assert(_edge_indexed);
            return _edges.size();
        }
        //}@

    private:
        /// Checks internal consistency
        void check() const;
};


template<typename EdgeInputIterator>
void BipartiteGraph::construct( size_t nr1, size_t nr2, EdgeInputIterator begin, EdgeInputIterator end ) {
    _nb1.clear();
    _nb1.resize( nr1 );
    _nb2.clear();
    _nb2.resize( nr2 );

    for( EdgeInputIterator e = begin; e != end; e++ ) {
#ifdef DAI_DEBUG
        addEdge( e->first, e->second, true );
#else
        addEdge( e->first, e->second, false );
#endif
    }
}


} // end of namespace dai


/** \example example_bipgraph.cpp
 *  This example deals with the following bipartite graph:
 *  \dot
 *  graph example {
 *    ordering=out;
 *    subgraph cluster_type1 {
 *      node[shape=circle,width=0.4,fixedsize=true,style=filled];
 *      12 [label="2"];
 *      11 [label="1"];
 *      10 [label="0"];
 *    }
 *    subgraph cluster_type2 {
 *      node[shape=polygon,regular=true,sides=4,width=0.4,fixedsize=true,style=filled];
 *      21 [label="1"];
 *      20 [label="0"];
 *    }
 *    10 -- 20;
 *    11 -- 20;
 *    12 -- 20;
 *    11 -- 21;
 *    12 -- 21;
 *  }
 *  \enddot
 *  It has three nodes of type 1 (drawn as circles) and two nodes of type 2 (drawn as rectangles). 
 *  Node 0 of type 1 has only one neighbor (node 0 of type 2), but node 0 of type 2 has three neighbors (nodes 0,1,2 of type 1).
 *  The example code shows how to construct a BipartiteGraph object representing this bipartite graph and
 *  how to iterate over nodes and their neighbors.
 *
 *  \section Output
 *  \verbinclude examples/example_bipgraph.out
 *
 *  \section Source
 */


#endif
