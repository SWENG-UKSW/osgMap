varying vec2 texCoord;

void main() {
    texCoord = gl_Vertex.xy * 0.2; 
    gl_Position = ftransform();
}
