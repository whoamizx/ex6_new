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

// Structure to represent a quadruple
struct Quadruple {
    std::string op;
    std::string arg1;
    std::string arg2;
    std::string result;
    
    Quadruple(std::string o = "", std::string a1 = "", std::string a2 = "", std::string r = "")
        : op(o), arg1(a1), arg2(a2), result(r) {}
};

// Node in the DAG representation
class DAGNode {
public:
    int id;
    std::string op;      // Operation or value
    int left;            // Left operand node ID
    int right;           // Right operand node ID
    std::vector<std::string> aliases; // Variables associated with this node
    
    DAGNode(int i, std::string o, int l = -1, int r = -1)
        : id(i), op(o), left(l), right(r) {}
    
    bool isLeaf() const { return left == -1 && right == -1; }
};

// The DAG optimizer class
class DAGOptimizer {
private:
    std::vector<DAGNode> nodes;
    std::unordered_map<std::string, int> varToNode; // Maps variable names to node IDs
    std::unordered_map<std::string, int> exprToNode; // Maps expression signatures to node IDs
    
    // Find a node with the same operation and operands
    int findCommonExpr(const std::string& op, int left, int right) {
        std::string key = op + "_" + std::to_string(left) + "_" + std::to_string(right);
        auto it = exprToNode.find(key);
        return it != exprToNode.end() ? it->second : -1;
    }
    
    // Register a new expression in our lookup map
    void registerExpr(const std::string& op, int left, int right, int nodeId) {
        std::string key = op + "_" + std::to_string(left) + "_" + std::to_string(right);
        exprToNode[key] = nodeId;
    }
    
    // Evaluate constant expressions
    bool evaluateConstant(const std::string& op, const std::string& arg1, const std::string& arg2, std::string& result) {
        // Check if both operands are constants
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
            if (val2 == 0) return false; // Avoid division by zero
            res = val1 / val2;
        }
        else return false; // Unknown operation
        
        result = std::to_string(res);
        return true;
    }
    
    // Get or create a node for a variable or constant
    int getNodeForValue(const std::string& value) {
        if (value.empty()) return -1; // Handle empty values
        
        // Check if it's a variable we already have a node for
        if (varToNode.find(value) != varToNode.end())
            return varToNode[value];
        
        // Create a new leaf node
        int id = nodes.size();
        nodes.emplace_back(id, value);
        
        // Only register as a variable if it's not a numeric constant
        if (!std::all_of(value.begin(), value.end(), 
                [](char c) { return std::isdigit(c) || c == '-'; })) {
            varToNode[value] = id;
            nodes[id].aliases.push_back(value);
        }
        
        return id;
    }

    // Safe node access
    DAGNode& getNode(int id) {
        if (id < 0 || id >= static_cast<int>(nodes.size())) {
            throw std::out_of_range("Node index out of range: " + std::to_string(id));
        }
        return nodes[id];
    }
    
public:
    // Build DAG from a list of quadruples
    void buildDAG(const std::vector<Quadruple>& quads) {
        for (const auto& quad : quads) {
            if (quad.op == "=") {
                // Assignment operation
                if (quad.arg1.empty()) {
                    // Handle special case of empty assignment
                    continue;
                }
                
                int srcNodeId = getNodeForValue(quad.arg1);
                varToNode[quad.result] = srcNodeId;
                getNode(srcNodeId).aliases.push_back(quad.result);
            } else {
                // Check for constant folding
                std::string constResult;
                if (evaluateConstant(quad.op, quad.arg1, quad.arg2, constResult)) {
                    // We can fold this to a constant
                    int constNodeId = getNodeForValue(constResult);
                    varToNode[quad.result] = constNodeId;
                    getNode(constNodeId).aliases.push_back(quad.result);
                } else {
                    // Regular operation - check for common subexpressions
                    int leftId = getNodeForValue(quad.arg1);
                    int rightId = quad.arg2.empty() ? -1 : getNodeForValue(quad.arg2);
                    
                    // Look for common subexpressions
                    int existingNodeId = findCommonExpr(quad.op, leftId, rightId);
                    if (existingNodeId != -1) {
                        // We found a common subexpression to reuse
                        varToNode[quad.result] = existingNodeId;
                        getNode(existingNodeId).aliases.push_back(quad.result);
                    } else {
                        // Create a new operation node
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
    
    // Generate optimized quadruples from DAG
    std::vector<Quadruple> generateQuadruples() {
        std::vector<Quadruple> result;
        if (nodes.empty()) return result;
        
        std::vector<bool> processed(nodes.size(), false);
        std::set<int> required; // Nodes that must be computed
        
        // First determine which nodes are needed
        for (const auto& entry : varToNode) {
            if (entry.second >= 0 && entry.second < static_cast<int>(nodes.size())) {
                required.insert(entry.second);
            }
        }
        
        // Helper function for topological traversal
        std::function<void(int)> processNode = [&](int nodeId) {
            if (nodeId < 0 || nodeId >= static_cast<int>(nodes.size()) || processed[nodeId]) return;
            
            DAGNode& node = nodes[nodeId];
            
            // Process dependencies first
            if (node.left != -1) processNode(node.left);
            if (node.right != -1) processNode(node.right);
            
            // Only generate code for operation nodes
            if (!node.isLeaf()) {
                // Get the operand variables
                std::string leftVar;
                if (node.left >= 0 && node.left < static_cast<int>(nodes.size()) && !nodes[node.left].aliases.empty()) {
                    leftVar = nodes[node.left].aliases[0];
                } else if (node.left >= 0 && node.left < static_cast<int>(nodes.size())) {
                    leftVar = nodes[node.left].op; // Use node value if no aliases
                }
                
                std::string rightVar;
                if (node.right >= 0 && node.right < static_cast<int>(nodes.size()) && !nodes[node.right].aliases.empty()) {
                    rightVar = nodes[node.right].aliases[0];
                } else if (node.right >= 0 && node.right < static_cast<int>(nodes.size())) {
                    rightVar = nodes[node.right].op; // Use node value if no aliases
                }
                
                // Generate the operation quadruple
                if (!node.aliases.empty()) {
                    result.push_back({node.op, leftVar, rightVar, node.aliases[0]});
                
                    // If this node has multiple aliases, generate copy operations
                    for (size_t i = 1; i < node.aliases.size(); ++i) {
                        result.push_back({"=", node.aliases[0], "", node.aliases[i]});
                    }
                }
            } else if (std::all_of(node.op.begin(), node.op.end(), 
                   [](char c) { return std::isdigit(c) || c == '-'; })) {
                // This is a constant leaf node
                for (const auto& alias : node.aliases) {
                    result.push_back({"=", node.op, "", alias});
                }
            } else if (node.aliases.size() > 1) {
                // This is a variable leaf node with multiple aliases
                std::string primaryVar = node.aliases[0];
                for (size_t i = 1; i < node.aliases.size(); ++i) {
                    result.push_back({"=", primaryVar, "", node.aliases[i]});
                }
            }
            
            processed[nodeId] = true;
        };
        
        // Process all required nodes
        for (int nodeId : required) {
            processNode(nodeId);
        }
        
        return result;
    }

    // Print the DAG structure for debugging
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

// Function to parse input quadruples
std::vector<Quadruple> parseQuadruples(const std::vector<std::string>& lines) {
    std::vector<Quadruple> quads;
    
    for (const auto& line : lines) {
        // Skip empty lines or lines without parentheses
        if (line.empty()) continue;
        
        size_t start = line.find('(');
        size_t end = line.find(')');
        if (start == std::string::npos || end == std::string::npos) continue;
        
        // Extract values between parentheses
        std::string quadData = line.substr(start + 1, end - start - 1);
        
        // Parse the four components
        std::vector<std::string> parts;
        std::istringstream ss(quadData);
        std::string part;
        
        while (std::getline(ss, part, ',')) {
            // Trim whitespace
            part.erase(0, part.find_first_not_of(" \t\r\n"));
            part.erase(part.find_last_not_of(" \t\r\n") + 1);
            parts.push_back(part);
        }
        
        // Ensure we have exactly 4 parts
        while (parts.size() < 4) parts.push_back("");
        
        quads.push_back({parts[0], parts[1], parts[2], parts[3]});
    }
    
    return quads;
}

// Function to print quadruples
void printQuadruples(const std::vector<Quadruple>& quads) {
    for (const auto& quad : quads) {
        std::cout << "(" << quad.op << ", " << quad.arg1 << ", " 
                  << quad.arg2 << ", " << quad.result << ")" << std::endl;
    }
}

// Function to read file content
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

int main() {
    std::vector<std::string> inputLines;
    std::string line;
    
    // Read input from stdin
    std::cout << "请输入四元式序列（输入空行结束）：" << std::endl;
    while (std::getline(std::cin, line) && !line.empty()) {
        inputLines.push_back(line);
    }
    
    // If no input was provided, try reading from test file
    if (inputLines.empty()) {
        inputLines = readFile("test/test1.txt");
        
        // If still empty, use hardcoded sample data
        if (inputLines.empty()) {
            inputLines = {
                "(*, A, B, T1)",
                "(/, 6, 2, T2)",
                "(-, T1, T2, T3)",
                "(=, T3, , X)",
                "(=, 5, , C)",
                "(*, A, B, T4)",
                "(=, 2, , C)",
                "(+, 18, C, T5)",
                "(*, T4, T5, T6)",
                "(=, T6, , Y)"
            };
        }
    }
    
    try {
        // Parse input
        std::vector<Quadruple> inputQuads = parseQuadruples(inputLines);
        
        if (inputQuads.empty()) {
            std::cerr << "Error: No valid quadruples found in input." << std::endl;
            return 1;
        }
        
        // Print input quadruples
        std::cout << "输入的四元式序列:" << std::endl;
        printQuadruples(inputQuads);
        std::cout << std::endl;
        
        // Create optimizer and build DAG
        DAGOptimizer optimizer;
        optimizer.buildDAG(inputQuads);
        
        // Print DAG for debugging
        std::cout << "DAG结构:" << std::endl;
        optimizer.printDAG();
        std::cout << std::endl;
        
        // Generate optimized quadruples
        std::vector<Quadruple> optimizedQuads = optimizer.generateQuadruples();
        
        // Output result
        std::cout << "优化后的四元式序列:" << std::endl;
        printQuadruples(optimizedQuads);
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "发生未知错误" << std::endl;
        return 1;
    }
    
    return 0;
}
