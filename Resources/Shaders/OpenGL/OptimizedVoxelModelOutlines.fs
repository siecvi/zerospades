varying vec3 fogDensity;

uniform vec3 fogColor;

void main() {
	// apply fog fading
	gl_FragColor.xyz = mix(vec3(0.0), fogColor, fogDensity);
	gl_FragColor.w = 1.0;

#if !LINEAR_FRAMEBUFFER
	// gamma correct
	gl_FragColor.xyz = sqrt(gl_FragColor.xyz);
#endif
}