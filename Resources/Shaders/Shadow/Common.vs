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

void PrepareForMapShadow(vec3 vertexCoord, vec3 normal) ;
void PrepareForModelShadow(vec3 vertexCoord, vec3 normal);
void PrepareForMapRadiosity(vec3 vertexCoord, vec3 normal);
void PrepareForRadiosityForMap_Map(vec3 vertexCoord, vec3 centerCoord, vec3 normal);

void PrepareForShadow(vec3 vertexCoord, vec3 normal) {
	PrepareForMapShadow(vertexCoord, normal);
	PrepareForModelShadow(vertexCoord, normal);
	PrepareForMapRadiosity(vertexCoord, normal);
}

// map uses specialized shadow coordinate calculation to avoid glitch
void PrepareForShadowForMap(vec3 vertexCoord, vec3 centerCoord, vec3 normal) {
	PrepareForMapShadow(centerCoord + normal * 0.1, normal);
	PrepareForModelShadow(vertexCoord, normal);
	PrepareForRadiosityForMap_Map(vertexCoord, centerCoord, normal);
}