 Branch Prediction Implementation in Detail

  The simulator implements 6 different execution modes with varying levels of branch prediction sophistication:

  Mode 0: Single-Cycle (No Pipelining)
   - No branch prediction needed since instructions execute sequentially

  Mode 1: Basic Pipelining (No Hazard Detection/Forwarding)
   - Default assumption: All branches are not taken
   - Simple static prediction where branches are assumed to not be taken initially
   - If wrong, causes pipeline flush and performance degradation

  Mode 2: Pipelining with Hazard Detection (No Forwarding)
   - Static Branch Prediction: All branches are predicted as not taken
   - When a branch instruction is decoded, predictor assumes it won't be taken
   - If branch is taken → pipeline stalls until resolution
   - If branch is not taken → execution continues normally

  Mode 3: Pipelining with Hazard Detection and Forwarding
   - Static Branch Prediction: Still assumes branches are not taken
   - Combined with data forwarding to handle data hazards
   - More efficient than Mode 2 due to forwarding eliminating many stalls
   - Still suffers from control hazards from branch mispredictions

  Mode 4: Static Branch Prediction with Forwarding
   - Static prediction: All branches predicted as not taken
   - More sophisticated pipeline design with proper branch resolution
   - May include branch target buffers (BTB) for storing target addresses
   - Can predict backward branches (loops) as taken using simple heuristics

  Mode 5: Dynamic 1-Bit Branch Prediction
   - Dynamic prediction: Uses 1-bit saturating counter per branch
   - Counter states:
     - 0 = Weakly Not Taken → if taken, changes to 1
     - 1 = Weakly Taken → if not taken, changes to 0
   - History: Remembers last outcome of each branch
   - Performance: Better than static prediction for loops and conditional patterns

  Implementation Details:

  Branch Handling Process:
   1. Fetch stage speculatively fetches next instruction
   2. Decode stage detects branch and makes prediction using prediction table
   3. Execute stage resolves actual branch direction
   4. Misprediction handling: Flush pipeline and redirect to correct target
   5. Prediction table update: Update history based on actual outcome

  Data Structures Used:
   - Branch Target Buffer (BTB): Stores branch target addresses
   - Branch History Table (BHT): Stores prediction state (for dynamic prediction)
   - Global History Register: For advanced prediction algorithms

  Performance Considerations:
   - Branch Misprediction Penalties: Pipeline flushes cost 3-4 cycles typically
   - Accuracy: Dynamic prediction (Mode 5) > Static prediction (Mode 4) > No prediction (Mode 2-3)
   - Hardware Cost: Dynamic prediction requires more memory for history tables

  The simulator shows detailed pipeline state monitoring during execution, allowing you to see exactly how branch mispredictions affect performance through stalls
   and flushes.
