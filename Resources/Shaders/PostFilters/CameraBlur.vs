/*
 Copyright (c) 2013 yvt
 
 This file is part of OpenSpades.
 
 OpenSpades is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 OpenSpades is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with OpenSpades.  If not, see <http://www.gnu.org/licenses/>.
 
 */

attribute vec2 positionAttribute;

uniform mat4 reverseMatrix;

varying vec2 newCoord;
varying vec3 oldCoord;

void main() {	
	vec2 pos = positionAttribute;
	vec2 scrPos = pos * 2.0 - 1.0;
	
	gl_Position = vec4(scrPos, 0.5, 1.0);
	
	newCoord = pos;
	
	vec2 cvtCoord = pos - 0.5;
	oldCoord = (reverseMatrix * vec4(cvtCoord, 0.0, 1.0)).xyz;
	oldCoord.xy += vec2(oldCoord.z) * 0.5;
}