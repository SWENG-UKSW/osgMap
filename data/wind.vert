varying vec2 texCoord;
uniform float osg_FrameTime;

void main() {
    texCoord = gl_Vertex.xy * 0.05;
    float speed = 0.8; 
    float strength = 0.02;
    texCoord.x += sin(osg_FrameTime * speed) * strength;
    texCoord.y += cos(osg_FrameTime * speed * 0.5) * strength;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}