glslc main.shader.vert -o main.vert.spv
glslc main.shader.frag -o main.frag.spv
glslc shadow.shader.vert -o shadow.vert.spv

cp main.vert.spv ../assets/shaders/
cp main.frag.spv ../assets/shaders/
cp shadow.vert.spv ../assets/shaders/