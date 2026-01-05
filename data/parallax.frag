uniform sampler2D baseTexture;
uniform sampler2D heightMap;
varying vec2 texCoord;
varying vec3 viewDir;

void main() {
    float parallaxScale = 0.08; 
    float normalStrength = 15.0; 
    
    float height = texture2D(heightMap, texCoord).r;
    vec2 p = viewDir.xy * height * parallaxScale;
    vec2 newUV = texCoord - p;

    float h_center = texture2D(heightMap, newUV).r;
    float h_right  = texture2D(heightMap, newUV + vec2(0.002, 0.0)).r;
    float h_up     = texture2D(heightMap, newUV + vec2(0.0, 0.002)).r;

    float dX = (h_center - h_right) * normalStrength;
    float dY = (h_center - h_up) * normalStrength;
    vec3 normal = normalize(vec3(dX, dY, 1.0));

    vec3 lightDir = normalize(vec3(1.0, 0.5, 0.5)); 
    
    float diff = max(dot(normal, lightDir), 0.0);
    diff = pow(diff, 3.0);
    diff = max(diff, 0.3);

    vec3 viewVec = normalize(vec3(0.0, 0.0, 1.0));
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewVec, reflectDir), 0.0), 64.0);
    
    vec4 texColor = texture2D(baseTexture, newUV);
    
    vec3 finalColor = texColor.rgb * diff + (vec3(1.0) * spec * h_center * 0.8);
    
    gl_FragColor = vec4(finalColor, texColor.a);
}
