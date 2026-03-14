uniform mat4 projectionViewModelMatrix;
uniform mat4 modelMatrix;
uniform vec3 modelOrigin;
uniform vec3 viewOriginVector;

// [x, y, z]
attribute vec3 positionAttribute;

varying vec3 fogDensity;

vec4 ComputeFogDensity(float poweredLength);

void main() {
	vec4 vertexPos = vec4(positionAttribute + modelOrigin, 1.0);
	
	gl_Position = projectionViewModelMatrix * vertexPos;

	vec2 horzRelativePos = (modelMatrix * vertexPos).xy - viewOriginVector.xy;
	float horzDistance = dot(horzRelativePos, horzRelativePos);
	fogDensity = ComputeFogDensity(horzDistance).xyz;
}