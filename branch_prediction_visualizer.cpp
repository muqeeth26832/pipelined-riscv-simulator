#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

// Branch prediction visualization tool
class BranchPredictionVisualizer {
private:
    struct BranchRecord {
        uint64_t pc;
        bool taken;
        bool predicted;
        std::string instruction;
        int cycle;
        std::string prediction_accuracy;
    };
    
    std::vector<BranchRecord> branch_history;
    int total_predictions = 0;
    int correct_predictions = 0;
    
public:
    void recordBranch(uint64_t pc, bool taken, bool predicted, const std::string& instr, int cycle) {
        BranchRecord record;
        record.pc = pc;
        record.taken = taken;
        record.predicted = predicted;
        record.instruction = instr;
        record.cycle = cycle;
        record.prediction_accuracy = (taken == predicted) ? "CORRECT" : "MISPREDICTED";
        
        branch_history.push_back(record);
        
        total_predictions++;
        if (taken == predicted) {
            correct_predictions++;
        }
    }
    
    double getAccuracy() const {
        return total_predictions > 0 ? (double)correct_predictions / total_predictions * 100.0 : 0.0;
    }
    
    void printBranchHistory() const {
        std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                           BRANCH HISTORY VISUALIZATION                      ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣\n";
        
        if (branch_history.empty()) {
            std::cout << "║ No branch instructions executed yet                                      ║\n";
            std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
            return;
        }
        
        for (size_t i = 0; i < branch_history.size() && i < 10; ++i) {  // Show last 10 branches
            const auto& record = branch_history[i];
            std::string status_color = (record.taken == record.predicted) ? 
                "\033[32m" : "\033[31m";  // Green for correct, red for mispredicted
            std::string reset_color = "\033[0m";
            
            std::cout << "║ Cycle " << std::setw(3) << record.cycle << ": ";
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << record.pc << std::dec;
            std::cout << " " << std::setw(8) << record.instruction;
            std::cout << " | ";
            std::cout << "Actual: " << (record.taken ? "TAKEN" : "NOT_TAKEN");
            std::cout << " | ";
            std::cout << "Pred: " << (record.predicted ? "TAKEN" : "NOT_TAKEN");
            std::cout << " | ";
            std::cout << status_color << record.prediction_accuracy << reset_color;
            std::cout << " " << std::string(38 - record.prediction_accuracy.length(), ' ') << "║\n";
        }
        
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║ Total Predictions: " << std::setw(3) << total_predictions;
        std::cout << " | Correct: " << std::setw(3) << correct_predictions;
        std::cout << " | Accuracy: " << std::fixed << std::setprecision(1) << getAccuracy() << "%";
        std::cout << " " << std::string(27 - std::to_string(total_predictions).length() - std::to_string(correct_predictions).length(), ' ') << "║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
    }
    
    void printBranchPredictionStats() const {
        std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    BRANCH PREDICTION PERFORMANCE                            ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣\n";
        
        double accuracy = getAccuracy();
        std::string accuracy_color = (accuracy >= 90.0) ? "\033[32m" : 
                                    (accuracy >= 70.0) ? "\033[33m" : "\033[31m";
        std::string reset_color = "\033[0m";
        
        std::cout << "║ Overall Accuracy:      " << accuracy_color << std::fixed << std::setprecision(2) << accuracy << "%" << reset_color;
        std::cout << " " << std::string(42, ' ') << "║\n";
        std::cout << "║ Total Predictions:     " << std::setw(3) << total_predictions;
        std::cout << " " << std::string(49, ' ') << "║\n";
        std::cout << "║ Correct Predictions:   " << std::setw(3) << correct_predictions;
        std::cout << " " << std::string(49, ' ') << "║\n";
        std::cout << "║ Mispredictions:        " << std::setw(3) << total_predictions - correct_predictions;
        std::cout << " " << std::string(49, ' ') << "║\n";
        
        if (total_predictions > 0) {
            double misprediction_rate = (double)(total_predictions - correct_predictions) / total_predictions * 100.0;
            std::cout << "║ Misprediction Rate:    " << std::fixed << std::setprecision(2) << misprediction_rate << "%";
            std::cout << " " << std::string(47, ' ') << "║\n";
        }
        
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
    }
};

// Pipeline visualization tool
class PipelineVisualizer {
private:
    struct StageInfo {
        std::string name;
        std::string color;
        std::string instruction;
        std::string details;
        bool active;
    };
    
public:
    void printPipelineStatus(int cycle, int mode, 
                           const std::string& if_instr, const std::string& if_details,
                           const std::string& id_instr, const std::string& id_details,
                           const std::string& ex_instr, const std::string& ex_details,
                           const std::string& mem_instr, const std::string& mem_details,
                           const std::string& wb_instr, const std::string& wb_details) {
        std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                           PIPELINE STATUS - CYCLE " << std::setw(3) << cycle << " (MODE " << mode << ")           ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣\n";
        
        // IF Stage (Blue)
        std::cout << "║ \033[34mIF\033[0m │ ";
        if (!if_instr.empty()) {
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << (cycle*4) << std::dec
                      << " │ " << std::setw(8) << std::setfill(' ') << std::left << if_instr
                      << " │ " << if_details;
        } else {
            std::cout << "EMPTY";
        }
        std::cout << std::string(60 - (if_instr.empty() ? 5 : 35 + if_details.length()), ' ') << " ║\n";
        
        // ID Stage (Green)
        std::cout << "║ \033[32mID\033[0m │ ";
        if (!id_instr.empty()) {
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << (cycle*4-4) << std::dec
                      << " │ " << std::setw(8) << std::setfill(' ') << std::left << id_instr
                      << " │ " << id_details;
        } else {
            std::cout << "EMPTY";
        }
        std::cout << std::string(60 - (id_instr.empty() ? 5 : 35 + id_details.length()), ' ') << " ║\n";
        
        // EX Stage (Yellow)
        std::cout << "║ \033[33mEX\033[0m │ ";
        if (!ex_instr.empty()) {
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << (cycle*4-8) << std::dec
                      << " │ " << std::setw(8) << std::setfill(' ') << std::left << ex_instr
                      << " │ " << ex_details;
        } else {
            std::cout << "EMPTY";
        }
        std::cout << std::string(60 - (ex_instr.empty() ? 5 : 35 + ex_details.length()), ' ') << " ║\n";
        
        // MEM Stage (Magenta)
        std::cout << "║ \033[35mMEM\033[0m│ ";
        if (!mem_instr.empty()) {
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << (cycle*4-12) << std::dec
                      << " │ " << std::setw(8) << std::setfill(' ') << std::left << mem_instr
                      << " │ " << mem_details;
        } else {
            std::cout << "EMPTY";
        }
        std::cout << std::string(60 - (mem_instr.empty() ? 5 : 35 + mem_details.length()), ' ') << " ║\n";
        
        // WB Stage (Cyan)
        std::cout << "║ \033[36mWB\033[0m │ ";
        if (!wb_instr.empty()) {
            std::cout << "PC: 0x" << std::setw(8) << std::setfill('0') << std::hex << (cycle*4-16) << std::dec
                      << " │ " << std::setw(8) << std::setfill(' ') << std::left << wb_instr
                      << " │ " << wb_details;
        } else {
            std::cout << "EMPTY";
        }
        std::cout << std::string(60 - (wb_instr.empty() ? 5 : 35 + wb_details.length()), ' ') << " ║\n";
        
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
    }
};

// Test harness for branch prediction
class BranchPredictionTestHarness {
private:
    BranchPredictionVisualizer visualizer;
    PipelineVisualizer pipe_visualizer;
    
public:
    void runInteractiveTest() {
        std::cout << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                  RISC-V BRANCH PREDICTION TEST HARNESS                      ║\n";
        std::cout << "╠════════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║ This test harness will help you visualize and test:                         ║\n";
        std::cout << "║ • Static branch prediction (Mode 4)                                       ║\n";
        std::cout << "║ • Dynamic branch prediction (Mode 5)                                      ║\n";
        std::cout << "║ • Pipeline effects of branch mispredictions                                 ║\n";
        std::cout << "║ • Performance metrics and statistics                                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════════════════╝\n";
        
        std::cout << "\nSample branch test program:\n";
        std::cout << "  lui x3, 0x10000\n";
        std::cout << "  ld x10, 0(x0)\n";
        std::cout << "  sd x10, 0(x3)\n";
        std::cout << "  addi x10, x10, 23\n";
        std::cout << "  addi x10, x10, 27\n";
        std::cout << "  blt x0, x10, label   # Branch taken (x0 < x10)\n\n";
        
        std::cout << "Testing with different prediction modes:\n\n";
        
        // Simulate some branch predictions
        for (int cycle = 1; cycle <= 12; ++cycle) {
            if (cycle < 8) {
                // First few cycles, no branches yet
                pipe_visualizer.printPipelineStatus(
                    cycle, 5, 
                    cycle <= 6 ? "LUI" : "BLT", "0x100001b7",
                    cycle >= 2 ? (cycle < 7 ? "LD" : "ADDI") : "", cycle >= 2 ? "0x35030000" : "",
                    cycle >= 3 ? (cycle < 7 ? "SD" : "ADDI") : "", cycle >= 3 ? "0xa1b02300" : "",
                    cycle >= 4 ? (cycle < 7 ? "ADDI" : "BLT") : "", cycle >= 4 ? "0x17505130" : "",
                    cycle >= 5 ? (cycle < 7 ? "ADDI" : (cycle == 7 ? "BR_PRED_NT" : "BR_TAKEN")) : "", 
                    cycle >= 5 ? (cycle < 7 ? "0x1b505130" : (cycle == 7 ? "[PRED: NT]" : "[MISPRED]")) : ""
                );
            } else {
                // Branch prediction at work
                if (cycle == 8) {
                    visualizer.recordBranch(0x14, true, false, "BLT", cycle);
                    pipe_visualizer.printPipelineStatus(
                        cycle, 5, 
                        "", "",
                        "", "",
                        "", "",
                        "BLT", "[PRED: NT] -> MISPREDICTED",
                        "FLUSH", "Pipeline flushed"
                    );
                } else if (cycle <= 10) {
                    pipe_visualizer.printPipelineStatus(
                        cycle, 5, 
                        "LUI", "0x100001b7 (from target)",
                        "", "",
                        "", "",
                        "", "",
                        "", ""
                    );
                } else {
                    visualizer.recordBranch(0x14, true, true, "BLT", cycle);
                    pipe_visualizer.printPipelineStatus(
                        cycle, 5, 
                        "BLT", "[PRED: TAKEN]",
                        "BLT", "0xfea046e3",
                        "", "",
                        "", "",
                        "BR_RES", "[BR_TAKEN]"
                    );
                }
            }
            
            // Show branch history periodically
            if (cycle % 4 == 0) {
                visualizer.printBranchHistory();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        visualizer.printBranchPredictionStats();
    }
};

int main() {
    std::cout << "RISC-V Branch Prediction Visualization Tool\n";
    std::cout << "============================================\n";
    
    BranchPredictionTestHarness harness;
    harness.runInteractiveTest();
    
    std::cout << "\nTo run with real simulator:\n";
    std::cout << "  ./vm --pipelined 4 --run <program.s>  # Static prediction\n";
    std::cout << "  ./vm --pipelined 5 --run <program.s>  # Dynamic prediction\n";
    
    return 0;
}