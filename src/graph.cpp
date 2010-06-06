/*
 * Mycelia immersive 3d network visualization tool.
 * Copyright (C) 2008-2009 Sean Whalen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <graph.hpp>

using namespace std;

Graph::Graph(Mycelia* application): application(application)
{
    version = -1;
    nodeId = -1;
    edgeId = -1;
}

Graph& Graph::operator=(const Graph& g)
{
    application = g.application;
    version = g.version;
    
    nodeMap.clear();
    nodeMap.insert(g.nodeMap.begin(), g.nodeMap.end());
    
    nodes.clear();
    nodes.insert(g.nodes.begin(), g.nodes.end());
    
    edgeMap.clear();
    edgeMap.insert(g.edgeMap.begin(), g.edgeMap.end());
    
    edges.clear();
    edges.insert(g.edges.begin(), g.edges.end());
    
    return *this;
}

/*
 * general
 */
void Graph::clear()
{
    mutex.lock();
    
    nodes.clear();
    nodeMap.clear();
    nodeMap.rehash(1000);
    
    edges.clear();
    edgeMap.clear();
    edgeMap.rehash(1000);
    
    version = -1;
    nodeId = -1;
    edgeId = -1;
    
    mutex.unlock();
    application->clearSelections();
}

pair<Vrui::Point, Vrui::Scalar> Graph::locate()
{
    Vrui::Point center(0, 0, 0);
    Vrui::Scalar maxDistance = 0;
    int counted = 1;
    
    mutex.lock();
    
    foreach(int source, nodes)
    {
        if(!application->isSelectedComponent(source))
        {
            continue;
        }
        
        foreach(int target, nodes)
        {
            if(!application->isSelectedComponent(target))
            {
                continue;
            }
            
            Vrui::Scalar d = Geometry::mag(nodeMap[source].position - nodeMap[target].position);
            if(d > maxDistance) maxDistance = d;
        }
        
        center += (nodeMap[source].position - center) * (1.0 / counted);
        counted++;
    }
    
    mutex.unlock();
    
    if(maxDistance == 0) maxDistance = 30;
    return pair<Vrui::Point, Vrui::Scalar>(center, maxDistance);
}

int Graph::getVersion() const
{
    return version;
}

void Graph::randomizePositions(int radius)
{
    mutex.lock();
    
    foreach(int node, nodes)
    {
        float x = rand() % (2 * radius) - radius;
        float y = rand() % (2 * radius) - radius;
        float z = rand() % (2 * radius) - radius;
        nodeMap[node].position = Vrui::Point(x, y, z);
    }
    
    mutex.unlock();
}

void Graph::resetVelocities()
{
    mutex.lock();
    
    foreach(int node, nodes)
    {
        nodeMap[node].velocity = Vrui::Vector(0, 0, 0);
    }
    
    mutex.unlock();
}

void Graph::update()
{
    version++;
    Vrui::requestUpdate();
}

void Graph::write(const char* filename)
{
    mutex.lock();
    
    ofstream out(filename);
    out << "digraph G {" << endl;
    
    foreach(int node, nodes)
    {
        out << "  n" << node << "[ pos=\""
            << nodeMap[node].position[0] << ","
            << nodeMap[node].position[1] << ","
            << nodeMap[node].position[2] << "\" ];\n";
    }
    
    foreach(int edge, edges)
    {
        out << "  n" << edgeMap[edge].source << " -> n" << edgeMap[edge].target << ";\n";
    }
    
    out << "}\n";
    cout << "wrote " << filename << endl;
    
    mutex.unlock();
}

/*
 * edges
 */
int Graph::addEdge(int source, int target)
{
    mutex.lock();
    
    if(!isValidNode(source) || !isValidNode(target))
    {
        cout << "invalid node(s): " << source << " " << target << endl;
        mutex.unlock();
        return -1;
    }
    
    edgeId++;
    edges.insert(edgeId);
    edgeMap[edgeId] = Edge(source, target);
    
    nodeMap[source].outDegree++;
    nodeMap[target].inDegree++;
    nodeMap[source].adjacent[target].push_back(edgeId);
    
    mutex.unlock();
    update();
    
    return edgeId;
}

void Graph::clearEdges()
{
    mutex.lock();
    
    foreach(int node, nodes)
    {
        nodeMap[node].adjacent.clear();
    }
    
    edges.clear();
    edgeMap.clear();
    
    mutex.unlock();
    update();
}

int Graph::deleteEdge(int edge)
{
    mutex.lock();
    
    if(!isValidEdge(edge))
    {
        mutex.unlock();
        return -1;
    }
    
    Edge& e = edgeMap[edge];
    list<int>& neighbors = nodeMap[e.source].adjacent[e.target];
    neighbors.erase(find(neighbors.begin(), neighbors.end(), edge));
    
    edges.erase(edge);
    edgeMap.erase(edge);
    
    mutex.unlock();
    update();
    
    return edge;
}

const Edge& Graph::getEdge(int edge)
{
    return edgeMap[edge];
}

const list<int>& Graph::getEdges(int source, int target)
{
    return nodeMap[source].adjacent[target];
}

const std::string& Graph::getEdgeLabel(int edge)
{
    return edgeMap[edge].label;
}

float Graph::getEdgeWeight(int edge)
{
    return edgeMap[edge].weight;
}

const set<int>& Graph::getEdges() const
{
    return edges;
}

int Graph::getEdgeCount() const
{
    return (int)edges.size();
}

bool Graph::hasEdge(int source, int target)
{
    return nodeMap[source].adjacent.find(target) != nodeMap[source].adjacent.end();
}

bool Graph::isBidirectional(int edge)
{
    return isBidirectional(edgeMap[edge].source, edgeMap[edge].target);
}

bool Graph::isBidirectional(int source, int target)
{
    return hasEdge(source, target) && hasEdge(target, source);
}

bool Graph::isValidEdge(int edge) const
{
    return edgeMap.find(edge) != edgeMap.end();
}

void Graph::setEdgeLabel(int edge, const std::string& label)
{
    edgeMap[edge].label = string(label);
    
    update();
}

void Graph::setEdgeWeight(int edge, float weight)
{
    edgeMap[edge].weight = weight;
}

/*
 * nodes
 */
int Graph::addNode()
{
    mutex.lock();
    
    Node n;
    n.position = Vrui::Point(VruiHelp::randomFloat(), VruiHelp::randomFloat(), VruiHelp::randomFloat());
    
    nodeId++;
    nodes.insert(nodeId);
    nodeMap[nodeId] = n;
    
    mutex.unlock();
    update();
    
    return nodeId;
}

int Graph::addNode(const Vrui::Point& position)
{
    int id = addNode();
    setPosition(id, position);
    return id;
}

int Graph::addNode(const string& s)
{
    int id = addNode();
    setNodeLabel(id, s);
    return id;
}

int Graph::deleteNode()
{
    return deleteNode(*nodes.begin());
}

int Graph::deleteNode(int node)
{
    mutex.lock();
    
    if(!isValidNode(node))
    {
        mutex.unlock();
        return -1;
    }
    
    vector<int> killList(0);
    
    foreach(int edge, edges)
    {
        Edge& e = edgeMap[edge];
        
        if(e.source == node || e.target == node)
        {
            killList.push_back(edge);
        }
    }
    
    foreach(int edge, killList)
    {
        edges.erase(edge);
        edgeMap.erase(edge);
    }
    
    nodes.erase(node);
    nodeMap.erase(node);
    
    mutex.unlock();
    update();
    
    return node;
}

const Attributes& Graph::getAttributes(int node)
{
    return nodeMap[node].attributes;
}

int Graph::getComponent(int node)
{
    return nodeMap[node].component;
}

int Graph::getDegree(int node)
{
    return nodeMap[node].inDegree + nodeMap[node].outDegree;
}

const string& Graph::getNodeLabel(int node)
{
    return nodeMap[node].label;
}

GLMaterial* Graph::getMaterial(int node)
{
    return nodeMap[node].material.get();
}

const set<int>& Graph::getNodes() const
{
    return nodes;
}

int Graph::getNodeCount() const
{
    return (int)nodes.size();
}

const Vrui::Point& Graph::getPosition(int node)
{
    return nodeMap[node].position;
}

float Graph::getSize(int node)
{
    return nodeMap[node].size;
}

const Vrui::Point& Graph::getSourcePosition(int edge)
{
    return nodeMap[edgeMap[edge].source].position;
}

const Vrui::Point& Graph::getTargetPosition(int edge)
{
    return nodeMap[edgeMap[edge].target].position;
}

const Vrui::Vector& Graph::getVelocity(int node)
{
    return nodeMap[node].velocity;
}

bool Graph::isValidNode(int node) const
{
    return nodeMap.find(node) != nodeMap.end();
}

void Graph::setAttribute(int node, string& key, string& value)
{
    nodeMap[node].attributes.push_back(pair<string, string>(key, value));
}

void Graph::setColor(int node, int r, int g, int b, int a)
{
    setColor(node, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
}

void Graph::setColor(int node, double r, double g, double b, double a)
{
    nodeMap[node].material = boost::shared_ptr<GLMaterial>(new GLMaterial(GLMaterial::Color(r, g, b, a)));
    
    update();
}

void Graph::setNodeLabel(int node, const std::string& label)
{
    nodeMap[node].label = label;
    
    update();
}

void Graph::setPosition(int node, const Vrui::Point& position)
{
    nodeMap[node].position = position;
    
    update();
}

void Graph::setSize(int node, float size)
{
    nodeMap[node].size = size;
    
    update();
}

void Graph::updatePosition(int node, const Vrui::Vector& delta)
{
    nodeMap[node].position += delta;
    
    update();
}

// no update() needed
void Graph::updateVelocity(int node, const Vrui::Vector& delta)
{
    nodeMap[node].velocity += delta;
}

/*
 * boost wrappers
 */
boost::BoostGraph Graph::toBoost()
{
    boost::BoostGraph g;
    
    for(int node = 0; node > getNodeCount(); node++)
    {
        boost::add_vertex(g);
    }
    
    for(int edge = 0; edge < getEdgeCount(); edge++)
    {
        boost::add_edge(edgeMap[edge].source, edgeMap[edge].target, g);
    }
    
    return g;
}

// boost divides degree by 2, results won't match networkx python package
// http://lists.boost.org/boost-users/2008/11/42161.php
vector<double> Graph::getBetweennessCentrality()
{
    mutex.lock();
    
    boost::BoostGraph g = toBoost();
    vector<double> bc(getNodeCount());
    
    if(getNodeCount() == 0) return bc;
    brandes_betweenness_centrality(g,
            make_iterator_property_map(bc.begin(), boost::get(boost::vertex_index, g)));
            
    mutex.unlock();
    return bc;
}

vector<int> Graph::getShortestPath()
{
    mutex.lock();
    
    boost::BoostGraph g = toBoost();
    vector<int> p(getNodeCount());
    vector<int> d(getNodeCount());
    
    if(getNodeCount() == 0) return p;
    dijkstra_shortest_paths(g,
            application->getPreviousNode(),
            &p[0],
            &d[0],
            boost::get(boost::edge_weight, g),
            boost::get(boost::vertex_index, g),
            std::less<int>(), // compare
            boost::closed_plus<int>(), // combine
            std::numeric_limits<int>::max(), // max dist
            0, // zero dist
            boost::default_dijkstra_visitor());
            
    mutex.unlock();
    return p;
}

vector<int> Graph::getSpanningTree()
{
    mutex.lock();
    
    boost::BoostGraph g = toBoost();
    vector<int> p(getNodeCount());
    
    if(getNodeCount() == 0) return p;
    prim_minimum_spanning_tree(g, &p[0]);
    
    mutex.unlock();
    return p;
}

void Graph::setComponents()
{
    mutex.lock();
    
    boost::BoostGraph g = toBoost();
    vector<int> c(getNodeCount());
    
    connected_components(g, &c[0]);
    
    foreach(int node, nodes)
    {
        nodeMap[node].component = c[node];
    }
    
    mutex.unlock();
}