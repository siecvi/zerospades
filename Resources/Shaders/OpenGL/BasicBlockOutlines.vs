uniform mat4 projectionViewMatrix;
uniform mat4 viewMatrix;
uniform vec3 chunkPosition;
uniform vec3 viewOriginVector;

// [x, y, z]
attribute vec3 positionAttribute;

varying vec3 fogDensity;

vec4 ComputeFogDensity(float poweredLength);

void main() {
	vec4 vertexPos = vec4(positionAttribute + chunkPosition, 1.0);
	
	gl_Position = projectionViewMatrix * vertexPos;

	vec2 horzRelativePos = vertexPos.xy - viewOriginVector.xy;
	float horzDistance = dot(horzRelativePos, horzRelativePos);
	fogDensity = ComputeFogDensity(horzDistance).xyz;
}