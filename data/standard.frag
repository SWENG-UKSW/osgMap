uniform sampler2D baseTexture;
varying vec2 texCoord;

void main() {
    vec4 color = texture2D(baseTexture, texCoord);
    
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diff = max(dot(vec3(0.0, 0.0, 1.0), lightDir), 0.6);
    
    gl_FragColor = vec4(color.rgb * diff, color.a);
}
