#include "nsg.h"
#include "kgraph.h"
#include "mrng.h"
#include <stack>
#include<set>
#include <utility>
#include <algorithm>
#include <boost/unordered/unordered_map.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <coin/CbcModel.hpp>
#include <coin/OsiClpSolverInterface.hpp>


using UF_SET = boost::disjoint_sets< boost::associative_property_map< boost::unordered_map<int, int> >, boost::associative_property_map< boost::unordered_map<int, int> > >;

void print_cur_knn_components(unordered_map<int, unordered_set<int>>& cur_knn_components){
    cerr << " Current knn components: " << endl;
    for(auto& p: cur_knn_components){
        auto root = p.first;
        cerr << "root: " << root << " size: " << p.second.size() << " Elements: ";
        for(auto& e: p.second){
            cerr << " " << e;
        }
        cerr << endl;
    }

}

void print_usg(unordered_map<int, unordered_set<int>>& usg){
    cerr << "usg: " << endl;
    for(auto& p: usg){
        auto root = p.first;
        cerr << "root: " << root << " Neighbors: ";
        for(auto& e: p.second){
            cerr << " " << e;
        }
        cerr << endl;
    }
}

void print_uf(UF_SET& uf,
                int curq){
    cerr << "union-find sets: " << endl;
    unordered_map<int, unordered_set<int>> tmp;
    for(int i = 0; i < curq; ++i){
        auto root = uf.find_set(i);
        if(tmp.count(root) == 0){
            tmp[root] = unordered_set<int>();
            tmp[root].insert(i);
        }else{
            tmp[root].insert(i);
        }
    }
    for(auto& p: tmp){
        auto root = p.first;
        cerr << "root: " << root << " size: " << p.second.size() << " Elements: ";
        for(auto& e: p.second){
            cerr << " " << e;
        }
        cerr << endl;
    }
}

void print_combo3(unordered_map<int, unordered_set<int>>& cur_knn_components,
        unordered_map<int, unordered_set<int>>& usg,
        UF_SET& uf,
                int curq){
    print_cur_knn_components(cur_knn_components);
    print_usg(usg);
    print_uf(uf, curq);
}

int get_dim(const char* fname){
    ifstream fin(fname, ios::binary);
    int d;
    fin.read((char*)&d, 4);
    fin.close();
    return d;
}

vector<vector<int>>* read_ibin(const char* fname, int& n, int& d){
    ifstream fin(fname, ios::binary);
    if(!fin.is_open()){
        cerr << "Cannot open file " << fname << endl;
        exit(-1);
    }
    fin.read(reinterpret_cast<char*>(&n), sizeof(n));
    fin.read(reinterpret_cast<char*>(&d), sizeof(d));    
    std::cerr << "index: " << n << " * "  << d << endl;
    vector<vector<int>>* G = new vector<vector<int>>(n, vector<int>());
    for(int i = 0; i < n; i++){
        int _;
        for(int j = 0 ; j < d; j++){
            fin.read(reinterpret_cast<char*>(&_), sizeof(_));
            if(_ == -1) continue;;
            (*G)[i].push_back(_);
        }
    }
    fin.close();
    return G;
}

vector<int>* read_ibin_simple(const char* fname){
    cerr << "read from " << fname << endl;
    ifstream fin(fname, ios::binary);
    int n;
    // get n by the size of the file
    fin.seekg(0, ios::end);
    n = fin.tellg() / sizeof(int);
    fin.seekg(0, ios::beg);
    cerr << "n: " << n << endl;
    vector<int>* res = new vector<int>(n, 0);
    for(int i = 0; i < n; i++){
        int _;
        fin.read(reinterpret_cast<char*>(&_), sizeof(_));
        (*res)[i] = _;
    }
    fin.close();
    return res;
}

void write_ibin_simple(const char* fname, vector<int>& res){
    cerr << "write to " << fname << endl;
    ofstream fout(fname, ios::binary);
    for(auto& r: res){
        fout.write(reinterpret_cast<char*>(&r), sizeof(r));
    }
    fout.close();
}

vector<vector<unsigned>>* read_rev_graph(string graph_path, int n){
    vector<vector<unsigned>>* revG = new vector<vector<unsigned>>();
    ifstream fin(graph_path, ios::binary);
    if(!fin.is_open()){
        cerr << "Cannot open file " << graph_path << endl;
        exit(-1);
    }
    for(int i = 0; i < n; i++){
        vector<unsigned> tmp;
        unsigned k;
        fin.read((char*)&k, sizeof(unsigned));
        while(k != -1 && !fin.eof()){
            tmp.push_back(k);
            fin.read((char*)&k, sizeof(unsigned));
        }
        revG->push_back(tmp);
    }
    fin.close();
    return revG;
}

void get_rev_graph(vector<vector<int>>&G,vector<vector<int>>&edges, vector<vector<int>>&revedges){
    revedges.resize(G.size(), vector<int>());
    for(int i = 0; i < G.size(); ++i){
        for(int j = 0; j < G[i].size(); ++j){
            int v=  G[i][j];
            revedges[v].push_back(edges[i][j]);
        }
    }
}

void get_mrng_positions(vector<vector<int>>& MRNG, vector<vector<int>>& kgraph, vector<int>& res){
    vector<vector<int>> positions(MRNG.size(), vector<int>());
    int index = 0;
    #pragma omp parallel for
    for(int i = 0; i < MRNG.size(); ++i){
        #pragma omp critical
        {
            if(++index % 100000 == 0){
                std::cerr << "cur: " << index << endl;
            }
        }
        positions[i].resize(MRNG[i].size(), -1);
        int index_i = 0, index_j = 0;
        while (index_i < MRNG[i].size())
        {
            while((kgraph[i][index_j] == i) || (index_j < kgraph[i].size() && MRNG[i][index_i] != kgraph[i][index_j])){
                ++index_j;
            }
            positions[i][index_i] = (index_j + 1);
            ++index_i;
            ++index_j;
        }
    }

    // put posititions into res
    for(int i = 0; i < MRNG.size(); ++i){
        move(positions[i].begin(), positions[i].end(), back_inserter(res));
    }
}

// Recursive function to find all paths
void findAllPaths(const vector<vector<int>>& graph,
                      int current, const std::unordered_set<int>& terminalNodes,
                      std::vector<int>& path, std::unordered_set<int>& visited,
                      vector<int>&ends, unordered_map<int, vector<int>>& points_in_which_paths) {
    // Add the current node to the path and mark it as visited
    path.push_back(current);
    visited.insert(current);

    // Check if the current node is a terminal node
    if (terminalNodes.find(current) != terminalNodes.end()) {
        // If it is, save the current path
        ends.push_back(current);
        for(auto& p: path){
            points_in_which_paths[p].push_back(ends.size() - 1);
        }
        // Do not return; continue searching for other terminal nodes
    }

    // Continue exploring the neighbors
    for (int neighbor : graph[current]) {
        if (visited.count(neighbor) == 0) {
            findAllPaths(graph, neighbor, terminalNodes, path, visited, ends, points_in_which_paths);
        }
    }

    // Backtrack: remove the current node from the path and unmark it as visited
    path.pop_back();
    visited.erase(current);
}


void get_reachable_in_usg(int root, unordered_map<int, unordered_set<int>>& cur_knn_components,
                        unordered_map<int, unordered_set<int>>& usg, UF_SET & uf, unordered_set<int>& reachable) {
    std::unordered_set<int> visited;
    reachable.insert(cur_knn_components[root].begin(), cur_knn_components[root].end());
    visited.insert(root);
    std::stack<int> stack;
    stack.push(root);
    while (!stack.empty()) {
        int node = stack.top();
        stack.pop();
        unordered_set<int> neighbors(usg[node]);

        for (int neighbor : neighbors) {
            int neighbor_root = uf.find_set(neighbor);
            if (neighbor_root != neighbor) {
                usg[node].erase(neighbor);
                usg[node].insert(neighbor_root);
            }
            if (visited.count(neighbor_root)) {
                continue;
            }
            visited.insert(neighbor_root);
            stack.push(neighbor_root);
            if (cur_knn_components.count(neighbor_root)) {
                reachable.insert(cur_knn_components[neighbor_root].begin(),
                                 cur_knn_components[neighbor_root].end());
            }
        }
    }
}

void get_reachable_in_subgraph(vector<vector<int>>& G, int q, unordered_set<int>& V, unordered_set<int>& visited_V){
    // we only care the subgraph induced by V
    unordered_set<int>visited;
    visited_V.insert(q);
    queue<int> queue;
    queue.push(q);
    while(!queue.empty() && visited_V.size() < V.size()){
        auto vertex = queue.front();
        queue.pop();
        
        for(auto v: G[vertex]){
            if(V.count(v) > 0){
                visited_V.insert(v);
                if(visited.count(v) == 0){
                    queue.push(v);
                    visited.insert(vertex);
                }
            }
        }
    }
}

void get_reachable_in_V_limited(vector<vector<int>>& G, int limit_index, 
                        int q, unordered_set<int>& V, unordered_set<int>& visited_V){
    // we only care whether points in V can be reached from q
    unordered_set<int>visited;
    visited_V.insert(q);
    queue<int> queue;
    queue.push(q);
    while(!queue.empty() && visited_V.size() < V.size()){
        auto vertex = queue.front();
        queue.pop();

        int index = 0;
        for(auto v: G[vertex]){
            if(++index > limit_index)   break;
            if(v < 0) continue;
            if(V.count(v) > 0){
                visited_V.insert(v);
            }
            if(visited.count(v) == 0){
                queue.push(v);
                visited.insert(v);
            }
        }
    }
}

void get_reachable_in_V(vector<vector<int>>& G, int q, unordered_set<int>& V, unordered_set<int>& visited_V){
    // we only care whether points in V can be reached from q
    unordered_set<int>visited;
    visited_V.insert(q);
    queue<int> queue;
    queue.push(q);
    while(!queue.empty() && visited_V.size() < V.size()){
        auto vertex = queue.front();
        queue.pop();

        for(auto v: G[vertex]){
            if(V.count(v) > 0){
                visited_V.insert(v);
            }
            if(visited.count(v) == 0){
                queue.push(v);
                visited.insert(v);
            }
        }
    }
}

void init_usg(unordered_map<int, unordered_set<int>>& usg, vector<vector<int>>& G, unordered_set<int>& vk, unsigned* gt_list,
                unordered_map<int,int>& id2index){
    for(int i =0; i < vk.size(); ++i){
        unordered_set<int> tmp; // only points in vk will be inserted into tmp
        get_reachable_in_subgraph(G, gt_list[i], vk, tmp);
        for(auto v: tmp){
            auto index = id2index[v];
            if(i == index)  continue;
            usg[i].insert(index);
        }
    }
}

void dfs_in_usg(int node, vector<int>& path, unordered_map<int, int>& path_indexes, vector<int>& circle, 
        unordered_set<int>& visited, unordered_map<int, unordered_set<int>>& usg,
        UF_SET & uf) {
        visited.insert(node);
        path.push_back(node);
        path_indexes[node] = path.size() - 1;
        unordered_set<int> neighbor_set(usg[node]);

        for (int neighbor: neighbor_set) {
            int neighbor_root = uf.find_set(neighbor);
            if (neighbor_root != neighbor) {
                usg[node].erase(neighbor);
                usg[node].insert(neighbor_root);
            }

            if (path_indexes.find(neighbor_root) != path_indexes.end()) {
                int cycle_start = path_indexes[neighbor_root];
                int cur_len = path.size() - cycle_start;
                if (cur_len > circle.size()) {
                    circle = vector<int>(path.begin() + cycle_start, path.end());
                }
            }
            else if (visited.find(neighbor_root) == visited.end()) {
                dfs_in_usg(neighbor_root, path, path_indexes, circle, visited, usg, uf);
            }
        }

        path_indexes.erase(node);
        path.pop_back();
    }


vector<int> find_circle_in_usg(unordered_map<int, unordered_set<int>>& usg,
        UF_SET & uf){
    unordered_set<int> visited;
    vector<int> circle;

    for(auto& p: usg){
        int node = p.first;
        if(visited.find(node) == visited.end()){
            vector<int> path;
            unordered_map<int, int> path_indexes;
            dfs_in_usg(node, path, path_indexes, circle, visited, usg, uf);
        }
    }

    return circle;

}

// NOTE: the edges pointing to the circle are not updated
void union_usg(vector<int>& circle, unordered_map<int, unordered_set<int>>& usg,
        UF_SET & uf) {
        // only update the rscc involved, other rsccs are not affected
        // merge the circle
        int first = circle[0];
        unordered_set<int> out_neighbors(usg[first]);
        for (int i = 1; i < circle.size(); i++) {
            uf.union_set(circle[i], first);  // who is the root depends on the size
            out_neighbors.insert(usg[circle[i]].begin(), usg[circle[i]].end());
        }

        unordered_set<int> clean_neighbors;
        int root = uf.find_set(first);
        
        for(auto &neighbor: out_neighbors){
            auto _ = uf.find_set(neighbor);
            if(_ != root)
                clean_neighbors.insert(_);
        }

        for (int i = 0; i < circle.size(); i++) {
            usg.erase(circle[i]);
        }

        usg[root] = clean_neighbors;
}


bool check_finished_recall_prob(unordered_map<int, unordered_set<int>> &cur_knn_components, int recall, int prob,
                                unordered_map<int, unordered_set<int>>& usg,
                                UF_SET  &uf){
    // check whether the recall and prob are satisfied
    // if satisfied, return true
    // else return false
    unordered_map<int, unordered_set<int>> new_knn_components;
    for(auto& p: cur_knn_components){
        auto old_root = p.first;
        auto root = uf.find_set(old_root);
        if(new_knn_components.count(root) > 0){
            new_knn_components[root].insert(p.second.begin(), p.second.end());
        }else{
            new_knn_components[root] = unordered_set<int>(p.second.begin(), p.second.end());
        }
    }

    cur_knn_components = new_knn_components;

    int eligible_size = 0;
    for(auto& p: new_knn_components){
        auto root = p.first;
        unordered_set<int> reachable;
        get_reachable_in_usg(root, new_knn_components, usg, uf, reachable);
        if(reachable.size() >= recall){
            eligible_size += p.second.size();
        }
        if(eligible_size >= prob){
            return true;
        }
    }

    return eligible_size >= prob;

}


void get_delta0_max_knn_rscc_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, int GT_list_size, 
                                                vector<vector<unsigned>>& revG, int recall, int prob, 
                                                vector<int>&res){
    int current = 0;
#pragma omp parallel for
    for(int curq = 0; curq < GT_list.n ; ++curq){
#pragma omp critical
{
        if(++current % 100 == 0){
            std::cerr << "cur: " << current << endl;
        }
}
        auto gt_list = GT_list[curq];
        boost::unordered_map<int, int> rank_map;
        boost::unordered_map<int, int> parent_map;

        boost::associative_property_map< boost::unordered_map<int, int> > rank_pmap(rank_map);
        boost::associative_property_map< boost::unordered_map<int, int> > parent_pmap(parent_map);

        UF_SET  uf(rank_pmap, parent_pmap);
        unordered_map<int, unordered_set<int>> usg;
        unordered_set<int> vk;

        for(int i = 0; i < k; ++i){
            uf.make_set(i);
            usg[i] = unordered_set<int>();
            vk.insert(gt_list[i]);
        }
        unordered_map<int,int> id2index;
        for(int i = 0; i < GT_list.d; ++i){
            id2index[gt_list[i]] = i;
        }
        init_usg(usg, G, vk, gt_list, id2index);
        while(true){
            auto circle = find_circle_in_usg(usg, uf);
            assert(circle.size() != 1);
            if(circle.size() == 0)  break;
            union_usg(circle, usg, uf);
        }

        unordered_map<int, unordered_set<int>> cur_knn_components;
        for(int i = 0; i < k; ++i){
            auto root = uf.find_set(i);
            if(cur_knn_components.count(root) == 0){
                cur_knn_components[root] = unordered_set<int>();
                cur_knn_components[root].insert(i);
            }else{
                cur_knn_components[root].insert(i);
            }
        }        

        if(check_finished_recall_prob(cur_knn_components, recall, prob, usg, uf)){
            res[curq] = k;
            continue;
        }

        // print_combo3(cur_knn_components, usg, uf, k);


        set<int> Y(vk.begin(), vk.end());
        int i = k;
        for (; i < GT_list_size; i++) {
                int new_point = gt_list[i];
                set<int> out_neighbors_raw(G[new_point].begin(), G[new_point].end()), out_neighbors;
                set<int> in_neighbors_raw(revG[new_point].begin(), revG[new_point].end()), in_neighbors;
                set_intersection(out_neighbors_raw.begin(), out_neighbors_raw.end(), Y.begin(), Y.end(),
                                 inserter(out_neighbors, out_neighbors.begin()));
                set_intersection(in_neighbors_raw.begin(), in_neighbors_raw.end(), Y.begin(), Y.end(),
                                 inserter(in_neighbors, in_neighbors.begin()));
                Y.insert(new_point);
                set<int>out_neighbor_roots, in_neighbor_roots;
                for(auto& neighbor: out_neighbors){
                    out_neighbor_roots.insert(uf.find_set(id2index[neighbor]));
                }
                for(auto& neighbor: in_neighbors){
                    in_neighbor_roots.insert(uf.find_set(id2index[neighbor]));
                }
                uf.make_set(i);
                usg[i] = unordered_set<int>(out_neighbor_roots.begin(), out_neighbor_roots.end());

                vector<int>intersect;
                set_intersection(out_neighbor_roots.begin(), out_neighbor_roots.end(), in_neighbor_roots.begin(), in_neighbor_roots.end(), inserter(intersect, intersect.begin()));
                if(intersect.size() > 0){
                    intersect.push_back(i);
                    union_usg(intersect, usg, uf);
                }

                auto i_root = uf.find_set(i);
                for(auto & in_neighbor_root: in_neighbor_roots){
                    auto new_in_nei_root = uf.find_set(in_neighbor_root);
                    if(new_in_nei_root != i_root){
                        assert(usg.count(new_in_nei_root) > 0);
                        usg[new_in_nei_root].insert(i_root);
                    }
                }


                while(true){
                    auto circle = find_circle_in_usg(usg, uf);
                    assert(circle.size() != 1);
                    if(circle.size() == 0)  break;
                    union_usg(circle, usg, uf);
                }

                // print_combo3(cur_knn_components, usg, uf, i + 1);

                if(check_finished_recall_prob(cur_knn_components, recall, prob, usg, uf)){
                    break;
                }

            }
            if(i == GT_list_size)
                std::cerr << "Exceed the threshold: " << curq << endl;
            res[curq] = i + 1;
    }
}

void restore_usg_from_delta0_point(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                    UF_SET &uf, unordered_map<int, unordered_set<int>>& usg,
                                    unordered_map<int, unordered_set<int>>& cur_knn_components,
                                    unordered_map<int,int>& id2index){
        unordered_set<int> V;
        for(int i = 0; i < delta0_point; ++i){
            id2index[gt_list[i]] = i;
            uf.make_set(i);
            usg[i] = unordered_set<int>();
            V.insert(gt_list[i]);

        }
        init_usg(usg, G, V, gt_list, id2index);
        while(true){
            auto circle = find_circle_in_usg(usg, uf);
            assert(circle.size() != 1);
            if(circle.size() == 0)  break;
            union_usg(circle, usg, uf);
        }

        
        for(int i = 0; i < k; ++i){
            auto root = uf.find_set(i);
            if(cur_knn_components.count(root) == 0){
                cur_knn_components[root] = unordered_set<int>();
                cur_knn_components[root].insert(i);
            }else{
                cur_knn_components[root].insert(i);
            }
        }        
}

void get_reachable_pairs_from_delta_point_recall_prob(vector<vector<int>>& subG, int k, unsigned* gt_list, int delta0_point,
                                                    int recall, int prob, unordered_map<int,int>& id2index,
                                                    vector<pair<int, unordered_set<int>>>& ret){

        /* silly usg restored method
        boost::unordered_map<int, int> rank_map;
        boost::unordered_map<int, int> parent_map;

        boost::associative_property_map< boost::unordered_map<int, int> > rank_pmap(rank_map);
        boost::associative_property_map< boost::unordered_map<int, int> > parent_pmap(parent_map);
        UF_SET uf(rank_pmap, parent_pmap);
        unordered_map<int, unordered_set<int>> usg;
        unordered_map<int, unordered_set<int>> cur_knn_components;
        restore_usg_from_delta0_point(G, k, gt_list, delta0_point, uf, usg, cur_knn_components, id2index);

        int eligible_size = 0;
        for(auto &p: cur_knn_components){
            auto root = p.first;
            unordered_set<int> reachable;
            get_reachable_in_usg(root, cur_knn_components, usg, uf, reachable);
            if(reachable.size() >= recall){
                ret.emplace_back(p.second, reachable);
                eligible_size += p.second.size();
            }
            if(eligible_size >= prob){
                return;
            }
        }
        */


       // get the reachability of the first k points to the first k points
       
       unordered_set<int> V;
        for(int i = 0; i < k; ++i){
            V.insert(i);
        }
        // TODO: can be optimized by dynamic programming
        int eligible_size = 0;
       for(int i = 0 ; i < k && eligible_size < prob; ++i){
              unordered_set<int> reachable;
              get_reachable_in_V(subG, i, V, reachable);
              if(reachable.size() >= recall){
                  ret.emplace_back(i, reachable);
                  ++eligible_size;
              }
       }

       if(eligible_size >= prob)    return;

       cerr << "Can't find enough pairs, eligible_size: " << eligible_size << endl;
       ret.clear();
}

void get_subgraph(vector<vector<int>>& G, unsigned* gt_list, int delta0_point,
                    vector<vector<int>>& subgraph, unordered_map<int,int>& id2index){
    unordered_set<int> V;
    for(int i = 0; i < delta0_point; ++i){
        V.insert(gt_list[i]);
    }
    for(int i = 0; i < delta0_point; ++i){
        subgraph.push_back(vector<int>());

        for(auto& v: G[gt_list[i]]){
            if(V.count(v) > 0){
                subgraph[i].push_back(id2index[v]);
            }
        }
    }
}

void get_subgraph_place_holder(vector<vector<int>>& G, unsigned* gt_list, int delta0_point,
                    vector<vector<int>>& subgraph, unordered_map<int,int>& id2index){
    unordered_set<int> V;
    for(int i = 0; i < delta0_point; ++i){
        V.insert(gt_list[i]);
    }
    for(int i = 0; i < delta0_point; ++i){
        subgraph.push_back(G[gt_list[i]]);

        for(int j = 0; j < subgraph[i].size(); ++j){
            if(V.count(subgraph[i][j]) > 0){
                subgraph[i][j] = id2index[subgraph[i][j]];
            }else{
                subgraph[i][j] = -1;
            }
        }
    }
}

// deprecated: silly dijkstra
void dijkstra_shortest_paths(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            unordered_set<int>&points_in_paths){
    // the indexes of the subG is the indexes in gt_list
    // we collect the points in the shortest paths, without the terminals themselves
    int n = subG.size();
    vector<int>dist(n, INT_MAX);
    vector<int>prev(n, -1);  // Add a vector to keep track of the previous node in the shortest path
    vector<bool> visited(n, false);
    dist[source] = 0;
    int finish_size = 0;
    int dest_size = dest.size();
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    pq.push(make_pair(0, source));


    while(!pq.empty() && finish_size < dest_size){
        auto p = pq.top();
        pq.pop();
        auto d = p.first;
        auto u = p.second;

        if(visited[u]) continue;  // u has been visited
        visited[u] = true;

        if(dest.count(u) > 0){
            finish_size++;
        }

        for(auto& v: subG[u]){
            if(!visited[v] && dist[v] > dist[u] + 1){
                dist[v] = dist[u] + 1;
                prev[v] = u;  // Update the previous node for v
                pq.push(make_pair(dist[v], v));
            }
        }
    }

    // Build the shortest paths from the prev vector and collect the points in the paths
    for (int terminal : dest) {
        int v = prev[terminal];
        while(v != -1){
            points_in_paths.insert(v);
            v = prev[v];
        }
    }
}

void bfs_shortest_paths(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            unordered_set<int>&points_in_paths){
    // the indexes of the subG is the indexes in gt_list
    // we collect the points in the shortest paths, without the terminals themselves
    int n = subG.size();
    vector<int>dist(n, INT_MAX);
    vector<int>prev(n, -1);  // Add a vector to keep track of the previous node in the shortest path
    vector<bool> visited(n, false);
    dist[source] = 0;
    int finish_size = 0;
    int dest_size = dest.size();
    queue<int> q;
    q.push(source);


    while(!q.empty() && finish_size < dest_size){
        auto u = q.front();
        q.pop();

        if(visited[u]) continue;  // u has been visited
        visited[u] = true;

        if(dest.count(u) > 0){
            finish_size++;
        }

        for(auto& v: subG[u]){
            if(!visited[v] && dist[v] > dist[u] + 1){
                dist[v] = dist[u] + 1;
                prev[v] = u;  // Update the previous node for v
                q.push(v);
            }
        }
    }

    // Build the shortest paths from the prev vector and collect the points in the paths
    for (int terminal : dest) {
        int v = prev[terminal];
        while(v != -1){
            points_in_paths.insert(v);
            v = prev[v];
        }
    }


}

void node_weighted_dijkstra_shortest_paths(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            unordered_set<int>&points_in_paths, vector<int>& weight){
    // the indexes of the subG is the indexes in gt_list
    // we collect the points in the shortest paths, without the terminals themselves
    int n = subG.size();
    vector<int>dist(n, INT_MAX);
    vector<int>prev(n, -1);  // Add a vector to keep track of the previous node in the shortest path
    vector<bool> visited(n, false);
    dist[source] = 0;
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
    pq.push(make_pair(0, source));

    int finish_size = 0;
    int dest_size = dest.size();

    while(!pq.empty() && finish_size < dest_size){
        auto p = pq.top();
        pq.pop();
        auto d = p.first;
        auto u = p.second;

        if(visited[u]) continue;  // u has been visited (we might put the same node multi-times to pq)
        visited[u] = true;

        if(dest.count(u) > 0 ){
            finish_size++;
        }

        for(auto& v: subG[u]){
            int wei = weight[u];
            // if(points_in_paths.count(u) > 0){
            //     wei = 0;
            // }
            if(!visited[v] && dist[v] > dist[u] + wei){
                dist[v] = dist[u] + wei;
                prev[v] = u;  // Update the previous node for v
                pq.push(make_pair(dist[v], v));
            }
        }
    }

    for(int terminal: dest){
        int v = prev[terminal];
        while(v != -1){
            points_in_paths.insert(v);
            v = prev[v];
        }
    }

}


void node_weighted_dijkstra_shortest_paths_overlap(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            unordered_set<int>&points_in_paths, vector<int>& weight){
    // the indexes of the subG is the indexes in gt_list
    // we collect the points in the shortest paths, without the terminals themselves
    int n = subG.size();
    unordered_set<int> finished;
    int finish_size = 0;
    int dest_size = dest.size();

    while(finish_size < dest_size){
        vector<int>dist(n, INT_MAX);
        vector<int>prev(n, -1);  // Add a vector to keep track of the previous node in the shortest path
        vector<bool> visited(n, false);
        dist[source] = 0;
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
        pq.push(make_pair(0, source));

        int dest_point = -1;

        while(!pq.empty()){
            auto p = pq.top();
            pq.pop();
            auto d = p.first;
            auto u = p.second;

            if(visited[u]) continue;  // u has been visited (we might put the same node multi-times to pq)
            visited[u] = true;

            if(dest.count(u) > 0 && finished.count(u) == 0){
                finish_size++;
                finished.insert(u);
                dest_point = u;
                break;
            }

            for(auto& v: subG[u]){
                int wei = weight[u];
                if(points_in_paths.count(u) > 0){
                    wei = 0;
                }
                if(!visited[v] && dist[v] > dist[u] + wei){
                    dist[v] = dist[u] + wei;
                    prev[v] = u;  // Update the previous node for v
                    pq.push(make_pair(dist[v], v));
                }
            }
        }

        assert(dest_point != -1);
        int v = prev[dest_point];
        while(v != -1){
            points_in_paths.insert(v);
            v = prev[v];
        }

    }
}

void exact_node_weighted_ilp_path_based(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            vector<int>&weights, unordered_set<int>&points_in_paths){
    vector<int>path;
    unordered_set<int>visited;
    unordered_map<int, vector<int>>points_in_which_paths;
    vector<int>ends;
    dest.erase(source); // remove source from dest
    findAllPaths(subG, source, dest, path, visited, ends, points_in_which_paths);
    int numPaths = ends.size(), numNodes = subG.size();
    cerr << "numPaths: " << numPaths << endl;

    // Create a solver interface
    OsiClpSolverInterface solver;

    // Create variables
    // y variables for paths
    for (int j = 0; j < numPaths; ++j) {
        solver.addCol(CoinPackedVector(), 0.0, 1.0, 0.0); // Binary variable
    }
    // x variables for nodes
    for (int i = 0; i < numNodes; ++i) {
        solver.addCol(CoinPackedVector(), 0.0, 1.0, weights[i]); // Binary variable with weight as the coefficient
    }

    // Create constraints
    // Terminal connectivity constraints
    for (int t : dest) {
        CoinPackedVector constraint;
        for (int j = 0; j < numPaths; ++j) {
            if (ends[j] == t) {
                constraint.insert(j, 1.0); // y_j variable
            }
        }
        solver.addRow(constraint, 1.0, 1.0); // equal to 1
    }

    // Node-path consistency constraints
    for (int i = 0; i < numNodes; ++i) {
        CoinPackedVector constraint;
        for(auto &point: points_in_which_paths[i]){
            constraint.insert(point, 1.0); // y_j variable
        }
        constraint.insert(numPaths + i, -99999); // x_i variable with a negative coefficient
        solver.addRow(constraint, -solver.getInfinity(), 0.0); // Less than or equal to 0
    }

    // Set the objective function to minimize
    solver.setObjSense(1); // 1 for minimization
    
    // Solve the model
    solver.initialSolve();
    
    // Check if the model is solved
    if (solver.isProvenOptimal()) {
        const double *solution = solver.getColSolution();
        for(int i = 0; i < numNodes; ++i){
            if(solution[i + numPaths] > 0.5){
                points_in_paths.insert(i);
            }
        }
    }else{
        cerr << "No solution" << endl;
        exit(-2);
    }
}



void exact_ilp(vector<vector<int>>& subG, int source, unordered_set<int>&dest,
                            unordered_set<int>&points_in_paths){
    vector<vector<int>>edges(subG.size(), vector<int>());
    int num_edges = 0;
    for(int i = 0; i < subG.size(); ++i){
        for(auto& v: subG[i]){
            edges[i].push_back(num_edges++);
        }
    }
    vector<vector<int>>rev_edges;
    get_rev_graph(subG, edges, rev_edges);

    OsiClpSolverInterface solver;
    int num_pts = subG.size(), num_terminals = dest.size();
    for(int i = 0; i < num_pts; ++i){
        for(int end: subG[i]){
            solver.addCol(CoinPackedVector(), 0.0, 1.0, 1);
        }
    }

    for(int t = 0; t < num_terminals; ++t){
        for(int i = 0; i < num_pts; ++i){
            for(int end: subG[i]){
                solver.addCol(CoinPackedVector(), 0.0, 1.0, 0.0);
            }
        }
    }

    for(int i = 0 ; i < num_pts; ++i){
        if(i == source || dest.count(i) > 0) continue;
        for(int t = 0; t < num_terminals; ++t){
            CoinPackedVector row;
            for(int inEdge: rev_edges[i]){
                row.insert(num_edges + t * num_edges + inEdge, 1.0);
            }
            for(int outEdge: edges[i]){
                row.insert(num_edges + t * num_edges + outEdge, -1.0);
            }
            solver.addRow(row, 0.0, 0.0);
        }
    }

    for(int t = 0; t < num_terminals; ++t){
        CoinPackedVector row;
        for(int inEdge: rev_edges[source]){
            row.insert(num_edges + t * num_edges + inEdge, 1.0);
        }
        solver.addRow(row, 1.0, 1.0);
    }

    int t = 0;
    for(auto &d: dest){
        CoinPackedVector row;
        for(int inEdge: rev_edges[d]){
            row.insert(num_edges + t * num_edges + inEdge, 1.0);
        }
        solver.addRow(row, 1.0, 1.0);
        ++t;
    }

    for(t = 0; t< num_terminals; ++t){
        int cnt = 0;
        for(int i = 0; i < num_pts; ++i){
            for(int end: subG[i]){
                CoinPackedVector row;
                row.insert(cnt, -1.0);
                row.insert(num_edges + t * num_edges + cnt, 1.0);
                solver.addRow(row, -COIN_DBL_MAX, 0.0);
                cnt++;
            }
        }
    }

    CbcModel model(solver);
    model.branchAndBound();
    auto solution = model.getColSolution();
    int cnt = 0;
    for(int i = 0; i < num_pts; ++i){
        for(int end: subG[i]){
            if(solution[cnt] > 0.5){
                points_in_paths.insert(i);
                points_in_paths.insert(end);
            }
            ++cnt;
        }
    }


    // Assuming you have defined MyWeightedEdge, g, and endVertices somewhere
    // std::map<std::pair<std::string, std::string>, OsiClpSolverInterface> y;
    // std::map<std::pair<std::string, std::string>, int> y_indices;
    // OsiClpSolverInterface model;
    // int countvars = 0;
    // for (int i = 0; i < subG.size(); ++i) {
    //     for(auto end: subG[i]){
    //     std::pair<std::string, std::string> s = make_pair(to_string(i), to_string(end));
    //     model.addCol(CoinPackedVector(), 0.0, 1.0, 1);
    //     y[s] = model;
    //     y_indices[s] = countvars;
    //     countvars += 1;
    //     }
    // }

    // std::map<std::tuple<std::string, std::string, std::string>, OsiClpSolverInterface> f;
    // std::map<std::tuple<std::string, std::string, std::string>, int> f_indices;
    // for (int i = 0; i < subG.size(); ++i)  {
    //     for(auto end: subG[i]){
    //         for (const auto& t : dest) {
    //             std::tuple<std::string, std::string, std::string> s = std::make_tuple(to_string(t), to_string(i), to_string(end));
    //             model.addCol(CoinPackedVector(), 0.0, 1.0, 0.0);
    //             f[s] = model;
    //             f_indices[s] = countvars;
    //             countvars += 1;
    //         }
    //     }
    // }

    // for(auto& s_value: dest){
    //     string s = to_string(s_value);
    //     for(int i = 0; i < subG.size(); ++i){
    //         string v = to_string(i);
    //         if (i == source) {
    //             // Constraint incoming edges
    //             CoinPackedVector exprin;
    //             for (const auto& e : rev_subG[i]) {
    //                 exprin.insert(f_indices[std::make_tuple(s, to_string(e), to_string(e))], 1.0);
    //             }
    //             if (exprin.getNumElements() > 0) {
    //                 model.addRow(exprin, 'E', 0.0);
    //             }
    //             // Constraint outgoing edges
    //             CoinPackedVector exprout;
    //             for (const auto& e : g.outgoingEdgesOf(v)) {
    //                 exprout.insert(f[std::make_tuple(s, g.getEdgeSource(e), g.getEdgeTarget(e))].getColIndex(), 1.0);
    //             }
    //             if (exprout.getNumElements() > 0) {
    //                 model.addConstraint(exprout, 'E', 1.0);
    //             }
    //         }
    //     }
    // }


}

int get_me_exhaustive_max_from_delta0_point_recall_prob_atom(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                                    int recall, int prob){
                                // delta0_point = 391 + k;
    vector<pair<int, unordered_set<int>>> reachable_pairs;
    unordered_map<int,int> id2index;
    for(int i =0 ; i < delta0_point; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph(G, gt_list, delta0_point, subgraph, id2index);
    vector<int> weight(subgraph.size(), 0);
    for(int i = 0; i < subgraph.size(); ++i){
        weight[i] = G[gt_list[i]].size();
    }

    // obtain reachable pairs by bfs shortest path
    get_reachable_pairs_from_delta_point_recall_prob(subgraph, k, gt_list, delta0_point, recall, prob, id2index, reachable_pairs);

    if(reachable_pairs.size() == 0){
        if(delta0_point >= 10000)   return -delta0_point;
        unordered_set<int> V;
        for(int i = 0; i < delta0_point; ++i){
            V.insert(gt_list[i]);
            for(auto& v: G[gt_list[i]]){
                V.insert(v);
            }
        }
        return -V.size();
    }


    int max_Res = 0;
    for(auto& p: reachable_pairs){
        unordered_set<int> points_in_paths;
        auto& src = p.first;
        auto& dest = p.second;
        node_weighted_dijkstra_shortest_paths(subgraph, src, dest, points_in_paths, weight);
        // node_weighted_dijkstra_shortest_paths_overlap(subgraph, src, dest, points_in_paths, weight);
        // exact_node_weighted_ilp_path_based(subgraph, src, dest, weight, points_in_paths);
        int res = 0;

        unordered_set<int> me;
        // all neighbros on G of points in shortest paths should be in ME_exhausted
        // since in exhausted search, we must test them to select the best direction
        for(auto &p : points_in_paths){
            me.insert(gt_list[p]);
            me.insert(G[gt_list[p]].begin(), G[gt_list[p]].end());
        }

        // The terminals themselves are also in ME_exhausted
        for(auto &p:reachable_pairs){
            auto& dest = p.second;
            for(auto& d: dest){
                me.insert(gt_list[d]);
            }
        }
        res = me.size();
        max_Res = max(max_Res, res);

    }


    return max_Res;

}

void get_me_exhaustive_max_from_delta0_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, vector<int>& delta0_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        if(delta0_point[i] == GT_list.d + 1){
            delta0_point[i] = GT_list.d;
        }
        if(delta0_point[i] > GT_list.d){
            cerr << "Query id = " << i << "\tdelta0_point: " << delta0_point[i] <<  "\tGT_list.d: " << GT_list.d << endl;
            exit(-1);
        }
        auto me_exhaustive = get_me_exhaustive_max_from_delta0_point_recall_prob_atom(G, k, gt_list, delta0_point[i], recall, prob);
        if(me_exhaustive < 0){
            me_exhaustive = -me_exhaustive;
            cerr << "Query id = " << i << endl;
        }
        // else{
        //     cerr << "Query id = " << i << "\tme_exhaustive: " << me_exhaustive << endl;
        // }
        res[i] = me_exhaustive;
    }
}



int get_me_exhaustive_from_delta0_point_recall_prob_atom(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                                    int recall, int prob){
                                // delta0_point = 391 + k;
    vector<pair<int, unordered_set<int>>> reachable_pairs;
    unordered_map<int,int> id2index;
    for(int i =0 ; i < delta0_point; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph(G, gt_list, delta0_point, subgraph, id2index);
    vector<int> weight(subgraph.size(), 0);
    for(int i = 0; i < subgraph.size(); ++i){
        weight[i] = G[gt_list[i]].size();
    }

    // obtain reachable pairs by bfs shortest path
    get_reachable_pairs_from_delta_point_recall_prob(subgraph, k, gt_list, delta0_point, recall, prob, id2index, reachable_pairs);

    if(reachable_pairs.size() == 0){
        if(delta0_point >= 10000)   return -delta0_point;
        unordered_set<int> V;
        for(int i = 0; i < delta0_point; ++i){
            V.insert(gt_list[i]);
            for(auto& v: G[gt_list[i]]){
                V.insert(v);
            }
        }
        return -V.size();
    }


    unordered_set<int> points_in_paths;
    for(auto& p: reachable_pairs){
        auto& src = p.first;
        auto& dest = p.second;
        node_weighted_dijkstra_shortest_paths(subgraph, src, dest, points_in_paths, weight);
        // node_weighted_dijkstra_shortest_paths_overlap(subgraph, src, dest, points_in_paths, weight);
    }

    int res = 0;
    unordered_set<int>me;

    // all neighbros on G of points in shortest paths should be in ME_exhausted
    // since in exhausted search, we must test them to select the best direction
    for(auto &p : points_in_paths){
        // res += weight[p];
        me.insert(gt_list[p]);
        me.insert(G[gt_list[p]].begin(), G[gt_list[p]].end());
    }

    // The terminals themselves are also in ME_exhausted
    for(auto &p:reachable_pairs){
        auto& dest = p.second;
        for(auto& d: dest){
            // if(points_in_paths.count(d) == 0){
            //     res++;
            //     points_in_paths.insert(d);
            // }
            me.insert(gt_list[d]);
        }
    }

    res = me.size();

    return res;

}

void get_me_exhaustive_from_delta0_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, vector<int>& delta0_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        if(delta0_point[i] == GT_list.d + 1){
            delta0_point[i] = GT_list.d;
        }
        if(delta0_point[i] > GT_list.d){
            cerr << "Query id = " << i << "\tdelta0_point: " << delta0_point[i] <<  "\tGT_list.d: " << GT_list.d << endl;
            exit(-1);
        }
        auto me_exhaustive = get_me_exhaustive_from_delta0_point_recall_prob_atom(G, k, gt_list, delta0_point[i], recall, prob);
        if(me_exhaustive < 0){
            me_exhaustive = -me_exhaustive;
            cerr << "Query id = " << i << endl;
        }
        res[i] = me_exhaustive;
    }
}

int get_me_exhausted_max_from_delta0_point_recall_prob_atom(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                                    int recall, int prob){
    vector<pair<int, unordered_set<int>>> reachable_pairs;
    unordered_map<int,int> id2index;
    for(int i =0 ; i < delta0_point; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph(G, gt_list, delta0_point, subgraph, id2index);

    // obtain reachable pairs by bfs shortest path
    get_reachable_pairs_from_delta_point_recall_prob(subgraph, k, gt_list, delta0_point, recall, prob, id2index, reachable_pairs);

    if(reachable_pairs.size() == 0){
        if(delta0_point >= 10000)   return -delta0_point;
        unordered_set<int> V;
        for(int i = 0; i < delta0_point; ++i){
            V.insert(gt_list[i]);
            for(auto& v: G[gt_list[i]]){
                V.insert(v);
            }
        }
        return -V.size();
    }


    int max_me_size = 0;
    for(auto& p: reachable_pairs){
        unordered_set<int> points_in_paths;
        auto& src = p.first;
        auto& dest = p.second;
        bfs_shortest_paths(subgraph, src, dest, points_in_paths);
        // exact_ilp(subgraph, src, dest, points_in_paths);
        unordered_set<int>me_exhausted;

        // all neighbros on G of points in shortest paths should be in ME_exhausted
        // since in exhausted search, we must test them to select the best direction
        for(auto &p : points_in_paths){
            int p_id = gt_list[p];
            me_exhausted.insert(p_id);
            me_exhausted.insert(G[p_id].begin(), G[p_id].end());
        }
        for(auto& d: dest){
            me_exhausted.insert(gt_list[d]);
        }
        max_me_size = max(max_me_size, (int)me_exhausted.size());
    }

    return max_me_size;

}

void get_me_exhausted_max_from_delta0_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, vector<int>& delta0_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        if(delta0_point[i] == GT_list.d + 1){
            delta0_point[i] = GT_list.d;
        }
        if(delta0_point[i] > GT_list.d){
            cerr << "Query id = " << i << "\tdelta0_point: " << delta0_point[i] <<  "\tGT_list.d: " << GT_list.d << endl;
            exit(-1);
        }
        auto me_exhausted = get_me_exhausted_max_from_delta0_point_recall_prob_atom(G, k, gt_list, delta0_point[i], recall, prob);
        if(me_exhausted < 0){
            me_exhausted = -me_exhausted;
            cerr << "Query id = " << i << endl;
        }
        res[i] = me_exhausted;
    }
}

int get_me_exhausted_from_delta0_point_recall_prob_atom(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                                    int recall, int prob){
    vector<pair<int, unordered_set<int>>> reachable_pairs;
    unordered_map<int,int> id2index;
    for(int i =0 ; i < delta0_point; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph(G, gt_list, delta0_point, subgraph, id2index);

    // obtain reachable pairs by bfs shortest path
    get_reachable_pairs_from_delta_point_recall_prob(subgraph, k, gt_list, delta0_point, recall, prob, id2index, reachable_pairs);

    if(reachable_pairs.size() == 0){
        if(delta0_point >= 10000)   return -delta0_point;
        unordered_set<int> V;
        for(int i = 0; i < delta0_point; ++i){
            V.insert(gt_list[i]);
            for(auto& v: G[gt_list[i]]){
                V.insert(v);
            }
        }
        return -V.size();
    }


    unordered_set<int> points_in_paths;
    for(auto& p: reachable_pairs){
        auto& src = p.first;
        auto& dest = p.second;
        // bfs_shortest_paths(subgraph, src, dest, points_in_paths);
        exact_ilp(subgraph, src, dest, points_in_paths);
    }

    unordered_set<int>me_exhausted;

    // all neighbros on G of points in shortest paths should be in ME_exhausted
    // since in exhausted search, we must test them to select the best direction
    for(auto &p : points_in_paths){
        int p_id = gt_list[p];
        me_exhausted.insert(p_id);
        me_exhausted.insert(G[p_id].begin(), G[p_id].end());
    }

    // The terminals themselves are also in ME_exhausted
    for(auto &p:reachable_pairs){
        auto& dest = p.second;
        for(auto& d: dest){
            me_exhausted.insert(gt_list[d]);
        }
    }

    return me_exhausted.size();

}

void get_me_exhausted_from_delta0_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, vector<int>& delta0_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        if(delta0_point[i] == GT_list.d + 1){
            delta0_point[i] = GT_list.d;
        }
        if(delta0_point[i] > GT_list.d){
            cerr << "Query id = " << i << "\tdelta0_point: " << delta0_point[i] <<  "\tGT_list.d: " << GT_list.d << endl;
            exit(-1);
        }
        auto me_exhausted = get_me_exhausted_from_delta0_point_recall_prob_atom(G, k, gt_list, delta0_point[i], recall, prob);
        if(me_exhausted < 0){
            me_exhausted = -me_exhausted;
            cerr << "Query id = " << i << endl;
        }
        // else{
        //     cerr << "Query id = " << i << "\tme_exhausted: " << me_exhausted << endl;
        // }
        res[i] = me_exhausted;
    }
}


void get_me_exhausted_from_fixed_delta_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, int delta_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        auto me_exhausted = get_me_exhausted_from_delta0_point_recall_prob_atom(G, k, gt_list, k + delta_point, recall, prob);
        res[i] = me_exhausted;
    }
}

void get_me_exhaustive_from_fixed_delta_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, int delta_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        auto me_exhausted = get_me_exhaustive_from_delta0_point_recall_prob_atom(G, k, gt_list, k + delta_point, recall, prob);
        res[i] = me_exhausted;
    }
}

int get_me_from_delta0_point_recall_prob_atom(vector<vector<int>>& G, int k, unsigned* gt_list, int delta0_point,
                                                   int recall, int prob){
    vector<pair<int, unordered_set<int>>> reachable_pairs;
    unordered_map<int,int> id2index;
    for(int i =0 ; i < delta0_point; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph(G, gt_list, delta0_point, subgraph, id2index);

    // obtain reachable pairs by bfs shortest path
    get_reachable_pairs_from_delta_point_recall_prob(subgraph, k, gt_list, delta0_point, recall, prob, id2index, reachable_pairs);

    if(reachable_pairs.size() == 0){
        return delta0_point;
    }


    unordered_set<int> points_in_paths;
    for(auto& p: reachable_pairs){
        auto& src = p.first;
        auto& dest = p.second;
        bfs_shortest_paths(subgraph, src, dest, points_in_paths);
    }

    // The terminals themselves are also in ME_exhausted
    for(auto &p:reachable_pairs){
        auto& dest = p.second;
        for(auto& d: dest){
            points_in_paths.insert(d);
        }
    }

    return points_in_paths.size();

}


void get_me_from_delta0_point_recall_prob(vector<vector<int>>& G, int k, Matrix<unsigned> GT_list, vector<int>& delta0_point,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        if(delta0_point[i] == GT_list.d + 1){
            delta0_point[i] = GT_list.d;
        }
        if(delta0_point[i] > GT_list.d){
            cerr << "Query id = " << i << "\tdelta0_point: " << delta0_point[i] <<  "\tGT_list.d: " << GT_list.d << endl;
            exit(-1);
        }
        auto me = get_me_from_delta0_point_recall_prob_atom(G, k, gt_list, delta0_point[i], recall, prob);
        res[i] = me;
    }
}


int get_K0_from_fixed_delta_point_recall_prob_atom(vector<vector<int>>& KGraph, int k, int cand, unsigned* gt_list,
                                                    int recall, int prob){
    unordered_map<int,int> id2index;
    for(int i =0 ; i < cand; ++i){
        id2index[gt_list[i]] = i;
    }

    vector<vector<int>>subgraph;
    // obtain subgraph
    get_subgraph_place_holder(KGraph, gt_list, cand, subgraph, id2index);

    unordered_set<int> success, V;
    for(int i = 0; i < k; ++i){
        V.insert(i);
    }

    int K = 1;
    for(; K < KGraph[0].size() && success.size() < prob; ++K){
        for(int i = 0 ; i < k && success.size() < prob; ++i){
            if(success.count(i) > 0)    continue;
            unordered_set<int> reachable;
            get_reachable_in_V_limited(subgraph, K, i, V ,reachable);
            if(reachable.size() >= recall){
                success.insert(i);
            }
        }
    }

    if(success.size() < prob){
        cerr << "Can't find enough qulified starting points, success.size(): " << success.size() << endl; 
    }
    return K;                               
}

void get_K0_from_fixed_delta_point_recall_prob(vector<vector<int>>& KGraph, int k, float beta, Matrix<unsigned> GT_list,
                                                    int recall, int prob, vector<int>&res){
    
    int cur = 0;
    int cand = floor((1 + beta) * k);
#pragma omp parallel for
    for(int i = 0; i < GT_list.n; ++i){
        #pragma omp critical
        {
            if(++cur % 100 == 0){
                std::cerr << "cur: " << cur << endl;
            }
        }
        auto gt_list = GT_list[i];
        auto me = get_K0_from_fixed_delta_point_recall_prob_atom(KGraph, k, cand, gt_list, recall, prob);
        res[i] = me;
    }

}

int main(int argc, char * argv[]) {

    int method = 1; // 0: kgraph 1: hnsw 2: nsg 3: mrng 4:ssg 5:taumg
    int purpose = 1; // 0: get delta0^p@Acc 1: get me_exhausted 2: get me^p_delta_0@Acc 3: get K_0^p@Acc (only KGraph)
                     // 4: get me_fixed_delta_0@Acc
                     // 100: for mrng, get the positions distirbution
    string data_str = "deep";   // dataset name
    int subk=50;
    float recall = 0.98;
    float prob = 0.98;
    int recall_target = ceil(recall * subk);
    int prob_target = ceil(prob * subk);

    string base_path_str = "../data";
    string result_base_path_str = "../results";
    string exp_name = "";
    string index_postfix = "";
    string query_postfix = "";
    // string index_postfix = "";
    string shuf_postfix = "";

    string data_path_str = base_path_str + "/" + data_str + "/" + data_str + "_base.fvecs" + shuf_postfix;
    string GT_num_str = "";
    if(data_str.find("rand") != string::npos || data_str.find("gauss") != string::npos)
        GT_num_str = "_50000";
    else if(data_str.find("deep") != string::npos || data_str.find("sift") != string::npos)
        GT_num_str = "_20000";
    else if( data_str.find("word2vec") != string::npos || data_str.find("glove") != string::npos)
        GT_num_str = "_50000";
    string groundtruth_path_str = base_path_str + "/" + data_str + "/" + data_str 
    + "_benchmark_groundtruth" + GT_num_str + ".ivecs" + shuf_postfix ;
    string result_prefix_str = base_path_str + "/" + data_str + "/" + data_str ;
    int dim  = get_dim(data_path_str.c_str());
    cerr << " data dimension = " << dim << endl;
    L2Space space(dim);   
    cerr << "L2 space" << endl;
    string index_path_str, result_path_str, revg_path_str;

    freopen(const_cast<char*>(result_path_str.c_str()),"a",stdout);
    cout << "k: "<<subk << endl;;
    cerr << "ground truth path: " << groundtruth_path_str << endl;
    Matrix<unsigned> GT(const_cast<char*>(groundtruth_path_str.c_str()));
    cerr << "GT list: " << GT.n << " * " << GT.d << endl;
    // GT.n = 300;
    size_t k = GT.d;

    vector<int>res(GT.n, 0);


    if(method == 0){
        // kgraph
        index_postfix = "_clean";
        string kgraph_od = "_500";
        int Kbuild =475; 
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_self_groundtruth" + kgraph_od + ".ivecs" + index_postfix + shuf_postfix;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_K" + to_string(Kbuild)  + ".ibin" + index_postfix + query_postfix;
        cerr << "index path: " << index_path_str << endl; 
        KGraph *kgraph = new KGraph(index_path_str.c_str(), nullptr, &space, dim, 0, Kbuild);
        auto index = kgraph->get_graph();
        
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(Kbuild) + "_self_groundtruth.ivecs_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, kgraph->_size);
            get_delta0_max_knn_rscc_point_recall_prob(*index, subk, GT, k, *revg, recall_target, prob_target, res);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
        }else if (purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_K" + to_string(Kbuild) + ".ibin" + index_postfix + shuf_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_exhausted_from_delta0_point_recall_prob(*index, subk, GT, *delta0_point, recall_target, prob_target, res);
        }else if (purpose == 2){
            result_path_str = result_prefix_str + "_me_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_K" + to_string(Kbuild) + ".ibin" + index_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_from_delta0_point_recall_prob(*index, subk, GT, *delta0_point, recall_target, prob_target, res);
        }else if(purpose == 3){
            double beta = 0.30;
            result_path_str = result_prefix_str + "_K0_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_beta"+ to_string(beta).substr(0, 4) + ".ibin" + index_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            get_K0_from_fixed_delta_point_recall_prob(*index, subk, beta, GT, recall_target, prob_target, res);
        }else if(purpose == 4){
            int delta_point = 500;
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_delta_point"+ to_string(delta_point) + "_K" + to_string(Kbuild) + ".ibin" + index_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            get_me_exhausted_from_fixed_delta_point_recall_prob(*index, subk, GT, delta_point, recall_target, prob_target, res);
        }
    }else if(method == 1){
        // hnsw
        string ef_str = "500"; 
        int M = 16;
        index_postfix = "_plain";
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_ef" + ef_str 
                + "_M" + to_string(M) + "_hnsw.ibin" + index_postfix + shuf_postfix;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_ef" + ef_str + "_M" + to_string(M) + ".ibin_hnsw" + index_postfix + shuf_postfix + query_postfix;
        cerr << "index path: " << index_path_str << endl; 
        int n, d;
        auto hnsw = read_ibin(index_path_str.c_str(), n, d);
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_ef" + ef_str + "_M" + to_string(M) + "_hnsw.ibin" + index_postfix + "_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, n);
            get_delta0_max_knn_rscc_point_recall_prob(*hnsw, subk, GT, k, *revg, recall_target, prob_target, res);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
        }else if(purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_ef" + ef_str + "_M" + to_string(M) + ".ibin_hnsw" + index_postfix + shuf_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_exhaustive_from_delta0_point_recall_prob(*hnsw, subk, GT, *delta0_point, recall_target, prob_target, res);
        }
    }else if(method == 2){
        // nsg
        int L = 40, R = 32, C = 500;
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_L" + to_string(L) + "_R" + to_string(R) + "_C" + to_string(C) + ".nsg" + index_postfix + shuf_postfix;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_L" + to_string(L) + "_R" + to_string(R) + "_C" + to_string(C) + ".ibin_nsg" + index_postfix + shuf_postfix + query_postfix;
        cerr << "index path: " << index_path_str << endl; 
        auto nsg = new NSG();
        nsg->Load(index_path_str.c_str());
        
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_L" + to_string(L) + "_R" + to_string(R) + "_C" + to_string(C) + ".nsg_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, nsg->nd_);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
            get_delta0_max_knn_rscc_point_recall_prob(nsg->final_graph_, subk, GT, k, *revg, recall_target, prob_target, res);
        }else if(purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_L" + to_string(L) + "_R" + to_string(R) + "_C" + to_string(C) + ".ibin_nsg" + index_postfix + shuf_postfix + query_postfix;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_exhausted_from_delta0_point_recall_prob(nsg->final_graph_, subk, GT, *delta0_point, recall_target, prob_target, res);
        }

    }else if(method == 3){
        // mrng
        int K=2047;
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + ".mrng" ;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_K" + to_string(K) + ".ibin_mrng" ;
        cerr << "index path: " << index_path_str << endl; 
        auto mrng = new MRNG();
        mrng->Load(index_path_str.c_str());
        
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + ".mrng_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, mrng->_size);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
            get_delta0_max_knn_rscc_point_recall_prob(mrng->_graph, subk, GT, k, *revg, recall_target, prob_target, res);
        }else if(purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_K" + to_string(K) + ".ibin_mrng" ;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            // get_me_exhaustive_max_from_delta0_point_recall_prob(mrng->_graph, subk, GT, *delta0_point, recall_target, prob_target, res);
            get_me_exhaustive_from_delta0_point_recall_prob(mrng->_graph, subk, GT, *delta0_point, recall_target, prob_target, res);
        }else if(purpose == 4){
            int delta_point = 391;
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_delta_point"+ to_string(delta_point) + "_K" + to_string(K) + ".ibin_mrng";
            cerr << "result path: "<< result_path_str << endl;
            // get_me_exhausted_from_fixed_delta_point_recall_prob(mrng->_graph, subk, GT, delta_point, recall_target, prob_target, res);
            // get_me_exhaustive_from_fixed_delta_point_recall_prob(mrng->_graph, subk, GT, delta_point, recall_target, prob_target, res);

        }else if(purpose == 100){
            index_postfix = "";
            string kgraph_od = "_10000";
            int Kbuild = 10000; 
            index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_self_groundtruth" + kgraph_od + ".ivecs" + index_postfix + shuf_postfix;
            cerr << "index path: " << index_path_str << endl; 
            int n;
            auto kgraph_vec= read_kgraph(index_path_str,n, Kbuild );
            // KGraph *kgraph = new KGraph(index_path_str.c_str(), nullptr, &space, dim, 0, Kbuild);
            // auto kgraph_vec = kgraph->get_graph();

            result_path_str = result_prefix_str + "_positions_distribution.ibin_mrng" ;
            cerr << "result path: "<< result_path_str << endl;
            res.resize(kgraph_vec->size(), -1);
            get_mrng_positions(mrng->_graph, *kgraph_vec, res);
        }

    }else if(method == 4){
        // ssg
        int K=2047, alpha = 60;
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + "_alpha" + to_string(alpha) + ".ssg" ;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_K" + to_string(K) + "_alpha" + to_string(alpha) + ".ibin_ssg" ;
        cerr << "index path: " << index_path_str << endl; 
        auto ssg = new MRNG();
        ssg->Load(index_path_str.c_str());
        
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + "_alpha" + to_string(alpha) + ".ssg_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, ssg->_size);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
            get_delta0_max_knn_rscc_point_recall_prob(ssg->_graph, subk, GT, k, *revg, recall_target, prob_target, res);
        }else if(purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_K" + to_string(K) + "_alpha" + to_string(alpha)+ ".ibin_ssg" ;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_exhausted_from_delta0_point_recall_prob(ssg->_graph, subk, GT, *delta0_point, recall_target, prob_target, res);
        }
    }else if(method == 5){
        // tau-mg
        int K=2047;
        index_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + ".taumg" ;
        string delta0_path_str = result_prefix_str + "_delta0_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4) 
        + "_K" + to_string(K) + ".ibin_taumg" ;
        cerr << "index path: " << index_path_str << endl; 
        auto taumg = new MRNG();
        taumg->Load(index_path_str.c_str());
        
        if(purpose == 0){
            revg_path_str = base_path_str + "/" + data_str + "/" + data_str + "_K" + to_string(K) + ".taumg_reversed_std";
            cerr << "revG path:" << revg_path_str << endl;
            auto revg = read_rev_graph(revg_path_str, taumg->_size);
            result_path_str = delta0_path_str;
            cerr << "result path: "<< result_path_str << endl;
            get_delta0_max_knn_rscc_point_recall_prob(taumg->_graph, subk, GT, k, *revg, recall_target, prob_target, res);
        }else if(purpose == 1){
            result_path_str = result_prefix_str + "_me_exhausted_forall_point_recall" + to_string(recall).substr(0, 4) + "_prob" + to_string(prob).substr(0, 4)
            + "_K" + to_string(K) + ".ibin_taumg" ;
            cerr << "result path: "<< result_path_str << endl;
            auto delta0_point = read_ibin_simple(delta0_path_str.c_str());
            get_me_exhausted_from_delta0_point_recall_prob(taumg->_graph, subk, GT, *delta0_point, recall_target, prob_target, res);
        }
    }

    cerr << "program finished" << endl;
    write_ibin_simple(result_path_str.c_str(), res);

    return 0;
}
