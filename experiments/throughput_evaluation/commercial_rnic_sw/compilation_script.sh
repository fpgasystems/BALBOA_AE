# Simple script that compiles the source code for the commercial RNIC software throughput evaluation experiment.
SOURCE = "RNIC_Comp_Poll_Tester.cpp"
OUTPUT = "RNIC_Comp_Poll_Tester"

echo "Step 1: Compiling $SOURCE..."

# Run the compilation command
g++ -g "$SOURCE" -o "$OUTPUT" \
    -libverbs \
    -lrdmacm \
    -lboost_program_options \
    -lpthread

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful: ./$OUTPUT created."
    exit 0
else
    echo "Error: Compilation failed."
    exit 1
fi