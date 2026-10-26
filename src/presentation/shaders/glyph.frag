#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 accent;
    float mica;
    float dark;
    float surfaceMode;
    vec2 blurStep;
    vec4 backgroundRect;
    float backdropReady;
    float nativeBackdrop;
};
layout(binding=1) uniform sampler2D source;
layout(binding=2) uniform sampler2D wallpaperSource;
void main() {
    float mask = texture(source, qt_TexCoord0).a;
    vec2 uv = backgroundRect.xy + qt_TexCoord0 * backgroundRect.zw;
    vec2 d = blurStep * mix(0.7, 1.0, mica);
    // Fixed 9-tap GPU blur in monitor coordinates; moving a widget changes
    // the sampled pixels, never stretches a stale wallpaper into each glyph.
    vec3 paper = texture(wallpaperSource,uv).rgb * 0.25;
    paper += (texture(wallpaperSource,uv+vec2(d.x,0)).rgb + texture(wallpaperSource,uv-vec2(d.x,0)).rgb
            + texture(wallpaperSource,uv+vec2(0,d.y)).rgb + texture(wallpaperSource,uv-vec2(0,d.y)).rgb) * 0.125;
    paper += (texture(wallpaperSource,uv+d).rgb + texture(wallpaperSource,uv-d).rgb
            + texture(wallpaperSource,uv+vec2(d.x,-d.y)).rgb + texture(wallpaperSource,uv+vec2(-d.x,d.y)).rgb) * 0.0625;
    vec3 tint = mix(vec3(0.97),vec3(0.14),dark);
    paper = mix(tint, paper, backdropReady * (1.0-nativeBackdrop));
    float tintAmount = mix(mix(0.25,0.12,mica),mix(0.14,0.025,mica),surfaceMode);
    vec3 color = mix(paper,tint,tintAmount);
    color = mix(color,accent.rgb,0.015);
    float sheen = (1.0-qt_TexCoord0.y) * 0.08 * (1.0-mica) * (1.0-surfaceMode);
    color += sheen;
    float alpha = mask * mix(mix(0.82,0.98,mica),1.0,surfaceMode);
    alpha = mix(alpha,mask * mix(0.42,0.28,mica),nativeBackdrop);
    fragColor = vec4(color * alpha,alpha) * qt_Opacity;
}
