#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <fstream>
#include <sstream>

struct Quadruple {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
    
    Quadruple(std::string o = "", std::string a1 = "", std::string a2 = "", std::string r = "")
        : op(o), arg1(a1), arg2(a2), result(r) {}
};

class DAGNode {
public:
    int id;
    std::string op;  
    int left;         
    int right;          
    std::vector<std::string> aliases; 
    
    DAGNode(int i, std::string o, int l = -1, int r = -1)
        : id(i), op(o), left(l), right(r) {}
    
    bool isLeaf() const { return left == -1 && right == -1; }
};

class DAGOptimizer {
private:
    std::vector<DAGNode> nodes;
    std::unordered_map<std::string, int> varToNode; 
    std::unordered_map<std::string, int> exprToNode; 
    
    int findCommonExpr(const std::string& op, int left, int right) {
        std::string key = op + "_" + std::to_string(left) + "_" + std::to_string(right);
        auto it = exprToNode.find(key);
        return it != exprToNode.end() ? it->second : -1;
    }
    
    void registerExpr(const std::string& op, int left, int right, int nodeId) {
        std::string key = op + "_" + std::to_string(left) + "_" + std::to_string(right);
        exprToNode[key] = nodeId;
    }
    
    bool evaluateConstant(const std::string& op, const std::string& arg1, const std::string& arg2, std::string& result) {
        bool isNum1 = !arg1.empty() && std::all_of(arg1.begin(), arg1.end(), 
                [](char c) { return std::isdigit(c) || c == '-'; });
        bool isNum2 = arg2.empty() || std::all_of(arg2.begin(), arg2.end(),
                [](char c) { return std::isdigit(c) || c == '-'; });
        
        if (!isNum1 || !isNum2) return false;
        
        int val1 = std::stoi(arg1);
        int val2 = arg2.empty() ? 0 : std::stoi(arg2);
        int res = 0;
        
        if (op == "+") res = val1 + val2;
        else if (op == "-") res = val1 - val2;
        else if (op == "*") res = val1 * val2;
        else if (op == "/") {
            if (val2 == 0) return false;
            res = val1 / val2;
        }
        else return false;
        
        result = std::to_string(res);
        return true;
    }
    
    int getNodeForValue(const std::string& value) {
        if (value.empty()) return -1;
        
        if (varToNode.find(value) != varToNode.end())
            return varToNode[value];
        
        int id = nodes.size();
        nodes.emplace_back(id, value);
        
        if (!std::all_of(value.begin(), value.end(), 
                [](char c) { return std::isdigit(c) || c == '-'; })) {
            varToNode[value] = id;
            nodes[id].aliases.push_back(value);
        }
        
        return id;
    }

    DAGNode& getNode(int id) {
        if (id < 0 || id >= static_cast<int>(nodes.size())) {
            throw std::out_of_range("Node index out of range: " + std::to_string(id));
        }
        return nodes[id];
    }
    
public:
    void buildDAG(const std::vector<Quadruple>& quads) {
        for (const auto& quad : quads) {
            if (quad.op == "=") {
                if (quad.arg1.empty()) {
                    continue;
                }
                
                int srcNodeId = getNodeForValue(quad.arg1);
                varToNode[quad.result] = srcNodeId;
                getNode(srcNodeId).aliases.push_back(quad.result);
            } else {
                std::string constResult;
                if (evaluateConstant(quad.op, quad.arg1, quad.arg2, constResult)) {
                    int constNodeId = getNodeForValue(constResult);
                    varToNode[quad.result] = constNodeId;
                    getNode(constNodeId).aliases.push_back(quad.result);
                } else {
                    int leftId = getNodeForValue(quad.arg1);
                    int rightId = quad.arg2.empty() ? -1 : getNodeForValue(quad.arg2);
                    
                    int existingNodeId = findCommonExpr(quad.op, leftId, rightId);
                    if (existingNodeId != -1) {

                        varToNode[quad.result] = existingNodeId;
                        getNode(existingNodeId).aliases.push_back(quad.result);
                    } else {

                        int newNodeId = nodes.size();
                        nodes.emplace_back(newNodeId, quad.op, leftId, rightId);
                        registerExpr(quad.op, leftId, rightId, newNodeId);
                        varToNode[quad.result] = newNodeId;
                        getNode(newNodeId).aliases.push_back(quad.result);
                    }
                }
            }
        }
    }
    
    std::vector<Quadruple> generateQuadruples() {
        std::vector<Quadruple> result;
        if (nodes.empty()) return result;
        
        std::vector<bool> processed(nodes.size(), false);
        std::set<int> required;
        
        for (const auto& entry : varToNode) {
            if (entry.second >= 0 && entry.second < static_cast<int>(nodes.size())) {
                required.insert(entry.second);
            }
        }
        
        std::function<void(int)> processNode = [&](int nodeId) {
            if (nodeId < 0 || nodeId >= static_cast<int>(nodes.size()) || processed[nodeId]) return;
            
            DAGNode& node = nodes[nodeId];
            
            if (node.left != -1) processNode(node.left);
            if (node.right != -1) processNode(node.right);
            
            if (!node.isLeaf()) {
                std::string leftVar;
                if (node.left >= 0 && node.left < static_cast<int>(nodes.size()) && !nodes[node.left].aliases.empty()) {
                    leftVar = nodes[node.left].aliases[0];
                } else if (node.left >= 0 && node.left < static_cast<int>(nodes.size())) {
                    leftVar = nodes[node.left].op;
                }
                
                std::string rightVar;
                if (node.right >= 0 && node.right < static_cast<int>(nodes.size()) && !nodes[node.right].aliases.empty()) {
                    rightVar = nodes[node.right].aliases[0];
                } else if (node.right >= 0 && node.right < static_cast<int>(nodes.size())) {
                    rightVar = nodes[node.right].op;
                }
                
                if (!node.aliases.empty()) {
                    result.push_back({node.op, leftVar, rightVar, node.aliases[0]});
                
                    for (size_t i = 1; i < node.aliases.size(); ++i) {
                        result.push_back({"=", node.aliases[0], "", node.aliases[i]});
                    }
                }
            } else if (std::all_of(node.op.begin(), node.op.end(), 
                   [](char c) { return std::isdigit(c) || c == '-'; })) {
                for (const auto& alias : node.aliases) {
                    result.push_back({"=", node.op, "", alias});
                }
            } else if (node.aliases.size() > 1) {
                std::string primaryVar = node.aliases[0];
                for (size_t i = 1; i < node.aliases.size(); ++i) {
                    result.push_back({"=", primaryVar, "", node.aliases[i]});
                }
            }
            
            processed[nodeId] = true;
        };
        
        for (int nodeId : required) {
            processNode(nodeId);
        }
        
        return result;
    }

    void printDAG() {
        std::cout << "DAG Structure:" << std::endl;
        for (const auto& node : nodes) {
            std::cout << "Node " << node.id << ": op=" << node.op;
            
            if (node.left != -1) 
                std::cout << ", left=" << node.left;
            if (node.right != -1) 
                std::cout << ", right=" << node.right;
            
            std::cout << ", aliases=[";
            for (size_t i = 0; i < node.aliases.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << node.aliases[i];
            }
            std::cout << "]" << std::endl;
        }
        
        std::cout << "Variable to Node mappings:" << std::endl;
        for (const auto& entry : varToNode) {
            std::cout << entry.first << " -> Node " << entry.second << std::endl;
        }
    }
};

std::vector<Quadruple> parseQuadruples(const std::vector<std::string>& lines) {
    std::vector<Quadruple> quads;
    
    for (const auto& line : lines) {
        if (line.empty()) continue;
        
        size_t start = line.find('(');
        size_t end = line.find(')');
        if (start == std::string::npos || end == std::string::npos) continue;
        
        std::string quadData = line.substr(start + 1, end - start - 1);
        
        std::vector<std::string> parts;
        std::istringstream ss(quadData);
        std::string part;
        
        while (std::getline(ss, part, ',')) {
            part.erase(0, part.find_first_not_of(" \t\r\n"));
            part.erase(part.find_last_not_of(" \t\r\n") + 1);
            parts.push_back(part);
        }
        
        while (parts.size() < 4) parts.push_back("");
        
        quads.push_back({parts[0], parts[1], parts[2], parts[3]});
    }
    
    return quads;
}

void printQuadruples(const std::vector<Quadruple>& quads) {
    for (const auto& quad : quads) {
        std::cout << "(" << quad.op << ", " << quad.arg1 << ", " 
                  << quad.arg2 << ", " << quad.result << ")" << std::endl;
    }
}

std::vector<std::string> readFile(const std::string& filePath) {
    std::vector<std::string> lines;
    std::ifstream file(filePath);
    
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();
    }
    
    return lines;
}

std::vector<std::string> listFilesInDirectory(const std::string& directoryPath) {
    std::vector<std::string> files;
    std::string command = "ls " + directoryPath + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    
    if (pipe) {
        char buffer[128];
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != nullptr) {
                std::string file = buffer;
                if (!file.empty() && file[file.size() - 1] == '\n') {
                    file.erase(file.size() - 1);
                }
                files.push_back(directoryPath + "/" + file);
            }
        }
        pclose(pipe);
    }
    
    return files;
}

void ensureDirectoryExists(const std::string& dir) {
    std::string command = "mkdir -p " + dir + " 2>/dev/null";
    system(command.c_str());
}

void processFile(const std::string& inputFile, const std::string& outputDir) {
    std::string filename = inputFile;
    size_t lastSlash = filename.find_last_of('/');
    if (lastSlash != std::string::npos) {
        filename = filename.substr(lastSlash + 1);
    }
    
    std::vector<std::string> inputLines = readFile(inputFile);
    
    if (inputLines.empty()) {
        std::cerr << "Warning: File is empty: " << inputFile << std::endl;
        return;
    }
    
    try {
        std::vector<Quadruple> inputQuads = parseQuadruples(inputLines);
        
        if (inputQuads.empty()) {
            std::cerr << "Error: No valid quadruples found in file: " << inputFile << std::endl;
            return;
        }
        
        DAGOptimizer optimizer;
        optimizer.buildDAG(inputQuads);
        
        std::vector<Quadruple> optimizedQuads = optimizer.generateQuadruples();
        
        std::string outputFile = outputDir + "/" + filename;
        std::ofstream outFile(outputFile);
        
        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open output file: " << outputFile << std::endl;
            return;
        }

        // outFile << "Original Quadruples:" << std::endl;
        // for (const auto& quad : inputQuads) {
        //     outFile << "(" << quad.op << ", " << quad.arg1 << ", " 
        //             << quad.arg2 << ", " << quad.result << ")" << std::endl;
        // }
        
        // /outFile << std::endl << "Optimized Quadruples:" << std::endl;
        for (const auto& quad : optimizedQuads) {
            outFile << "(" << quad.op << ", " << quad.arg1 << ", " 
                    << quad.arg2 << ", " << quad.result << ")" << std::endl;
        }
        
        outFile.close();
        std::cout << "Processed file: " << inputFile << " -> " << outputFile << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing file " << inputFile << ": " << e.what() << std::endl;
    }
}

int main() {
    const std::string testDir = "test";
    const std::string outputDir = "test_out";
    
    ensureDirectoryExists(outputDir);
    
    std::vector<std::string> testFiles = listFilesInDirectory(testDir);
    
    if (testFiles.empty()) {
        std::cerr << "Error: No files found in the test directory." << std::endl;
        return 1;
    }
    
    std::cout << "Processing files from directory: " << testDir << std::endl;
    for (const auto& file : testFiles) {
        processFile(file, outputDir);
    }
    
    std::cout << "All files processed. Results written to: " << outputDir << std::endl;
    
    return 0;
}
