glslc shader.vert -o vert.spv
glslc shader.frag -o frag.spv
cp vert.spv ../assets/shaders/
cp frag.spv ../assets/shaders/