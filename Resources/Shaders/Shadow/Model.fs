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

uniform sampler2DShadow shadowMapTexture1;
uniform sampler2DShadow shadowMapTexture2;
uniform sampler2DShadow shadowMapTexture3;

varying vec4 shadowMapCoord1;
varying vec4 shadowMapCoord2;
varying vec4 shadowMapCoord3;

#define shadowMapViewPosZ shadowMapCoord1.w

float EvaluteModelShadow() {
	if (shadowMapViewPosZ > -12.0) {
		vec4 scoord = shadowMapCoord1.xyzw;
		float v = shadow2D(shadowMapTexture1, scoord.xyz).x;
		return v;
	} else if (shadowMapViewPosZ > -40.0) {
		vec4 scoord = shadowMapCoord2.xyzw;
		float v = shadow2D(shadowMapTexture2, scoord.xyz).x;
		return v;
	} else {
		vec4 scoord = shadowMapCoord3.xyzw;
		float v = shadow2D(shadowMapTexture3, scoord.xyz).x;
		return v;
	}
}