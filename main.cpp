#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#include <fstream>
#include <string>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "stb_image_write.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/constants.hpp>

const int width = 1280, height = 720;
const float samples = 4.0f;

#include "criticalMath.h"

bool sceneIntersection(Ray ray, std::vector<Object*> stuff, hitHistory &history){
	float stuff_dist = std::numeric_limits<float>::max();
	for(auto &object : stuff){
		float dist_i = 0.0f;
		if(object->intersect(ray, dist_i) && dist_i < stuff_dist){
			stuff_dist = dist_i;
			glm::vec3 hitPoint = ray.orig + ray.dir * dist_i;
			hitHistory gotHist(dist_i, hitPoint, object->getNormal(hitPoint), object->material);
			history = gotHist;
		}
	}
    return stuff_dist < std::numeric_limits<float>::max();
}

glm::vec3 clampRay(glm::vec3 col){
	glm::vec3 res = col;
	res.x = col.x < 0 ? 0 : col.x > 1 ? 1 : col.x;
	res.y = col.y < 0 ? 0 : col.y > 1 ? 1 : col.y;
	res.z = col.z < 0 ? 0 : col.z > 1 ? 1 : col.z;
	return res;
}

glm::vec3 cast_ray(Ray ray, std::vector<Object*> stuff, std::vector<Light> lights, size_t depth = 0) {
	float numericalMinimum = 1e-3f;
	glm::vec3 finalColor;
	hitHistory rayHistory;
    if (depth > 8 || !sceneIntersection(ray, stuff, rayHistory)) {
        return glm::vec3(0.0f, 0.0f, 0.0f); // BG color!
    }
	
	glm::vec3 reflect_dir = glm::normalize(glm::reflect(ray.dir, rayHistory.normal));
    glm::vec3 reflect_orig = glm::dot(reflect_dir, rayHistory.normal) < 0 ? rayHistory.hitPoint - rayHistory.normal * numericalMinimum : rayHistory.hitPoint + rayHistory.normal * numericalMinimum;
    glm::vec3 reflect_color = cast_ray(Ray(reflect_orig, reflect_dir), stuff, lights, depth + 1);
	
	float totalDt = 0.0f, totalSpecular = 0.0f;
	glm::vec3 lightColor;
	for(size_t i = 0; i < lights.size(); i++){
		glm::vec3 L = glm::normalize(lights[i].pos - rayHistory.hitPoint);
		float lightDist = glm::length(lights[i].pos - rayHistory.hitPoint);
		float attenuation = (1.0f + pow(lightDist / 32.0f, lights[i].intensity));
		
		Ray shadowRay(glm::dot(L,rayHistory.normal) < 0 ? rayHistory.hitPoint - rayHistory.normal * numericalMinimum : rayHistory.hitPoint + rayHistory.normal * numericalMinimum, L);
		hitHistory shadowHist;
		
        if (sceneIntersection(shadowRay, stuff, shadowHist) && glm::length(shadowHist.hitPoint - shadowRay.orig) < lightDist){
			continue;
		}
		
		totalDt += (lights[i].intensity * std::max(0.f, glm::dot(L, rayHistory.normal))) / attenuation;
		totalSpecular += (powf(std::max(0.0f, glm::dot(-glm::reflect(-L, rayHistory.normal), ray.dir)),rayHistory.obtMat->specualirity) * lights[i].intensity) / attenuation;
			
		lightColor += lights[i].color * attenuation;
	}
	switch(rayHistory.obtMat->type){
		case Reflective:
			finalColor = reflect_color * totalDt * rayHistory.obtMat->pbrCtrl.z * lightColor;
			break;
		case Checkered:
			finalColor = rayHistory.obtMat->returnCheckered(rayHistory.hitPoint) * totalDt *
						rayHistory.obtMat->pbrCtrl.x + (reflect_color * rayHistory.obtMat->pbrCtrl.z) 
						+ glm::vec3(1.0f) * 
						std::floor(totalSpecular) * 
						rayHistory.obtMat->pbrCtrl.y * lightColor;
			break;
		case SphereCheckered:
			finalColor = rayHistory.obtMat->returnSphereCheckered(rayHistory.normal) * totalDt *
						rayHistory.obtMat->pbrCtrl.x + (reflect_color * rayHistory.obtMat->pbrCtrl.z) 
						+ glm::vec3(1.0f) * 
						std::floor(totalSpecular) * 
						rayHistory.obtMat->pbrCtrl.y * lightColor;
			break;
		default:
			finalColor = rayHistory.obtMat->color * totalDt * 
						rayHistory.obtMat->pbrCtrl.x + glm::vec3(1.0f) * 
						std::floor(totalSpecular) * 
						rayHistory.obtMat->pbrCtrl.y * lightColor;
			break;
	}
	return clampRay(finalColor);
}

RGB convertVec(glm::vec3 d){
	return RGB(std::round(d.x * 255.0f), std::round(d.y * 255.0f), std::round(d.z * 255.0f));
}

glm::vec3 calculateWin(float fov, float x, float y){
	float i =  (2*(x + 0.5f)/(float)width  - 1)*tan(fov/2.0f)*width/(float)height;
    float j = -(2*(y + 0.5f)/(float)height - 1)*tan(fov/2.0f);
	return glm::vec3(i, j, -1);
}

int main() {
   RGB* data = new RGB[width * height];
   
   std::vector<Material> materials;
   materials.push_back(Material(glm::vec3(0.9, 0.01, 0.1), glm::vec3(0.5f, 0.2f, 0.3f), 0.9f, Checkered));
   materials.push_back(Material(glm::vec3(0.9, 0.01, 0.3), glm::vec3(0.8f, 0.3f, 0.4f), 1.2f, SphereCheckered));
   materials.push_back(Material(glm::vec3(0.95, 0.8, 0.9), glm::vec3(0.2f, 0.3f, 0.5f), 6.0f, Reflective));
   materials.push_back(Material(glm::vec3(0.8, 0.1, 0.2), glm::vec3(0.3f, 0.5f, 0.2f), 1.0f, Diffuse));
   materials.push_back(Material(glm::vec3(0.7, 0.1, 0.1), glm::vec3(0.5f, 0.4f, 0.6f), 1.2f, Diffuse));
   
   materials[0].setString("reddy");
   materials[1].setString("reddySphere");
   materials[2].setString("bluey");
   materials[3].setString("greeny");
   materials[4].setString("thingy");
   
   std::vector<Object*> stuff;
   stuff.push_back(new Sphere(glm::vec3(0.0f, -2.0f, -14.0f), 2.0f, materials[1]));
   
   stuff.push_back(new Sphere(glm::vec3(5.0f, -3.0f, -15.0f), 1.2f, materials[2]));
   stuff.push_back(new Sphere(glm::vec3(-3.0f, -3.0f, -10.0f), 1.2f, materials[2]));
   
   stuff.push_back(new Sphere(glm::vec3(3.2f, -3.0f, -9.4f), 1.2f, materials[2]));
   stuff.push_back(new Sphere(glm::vec3(-4.0f, -3.0f, -15.0f), 1.2f, materials[2]));
   stuff.push_back(new Sphere(glm::vec3(-6.0f, -3.0f, -11.0f), 1.2f, materials[2]));
   stuff.push_back(new Sphere(glm::vec3(6.0f, -3.0f, -11.0f), 1.2f, materials[2]));
   
   stuff.push_back(new Plane(glm::vec3(0.0f, -4.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), materials[0]));
   stuff.push_back(new Plane(glm::vec3(0.0f, 6.0f, -5.0f), glm::vec3(0.0f, -1.0f, 0.0f), materials[3]));
   
   stuff.push_back(new Plane(glm::vec3(17.0f, 0.0f, -5.0f), glm::vec3(-1.0f, 0.0f, 0.0f), materials[2]));
   stuff.push_back(new Plane(glm::vec3(-17.0f, 0.0f, -5.0f), glm::vec3(1.0f, 0.0f, 0.0f), materials[2]));
   
   stuff.push_back(new Plane(glm::vec3(0.0f, 0.0f, -24.0f), glm::vec3(0.0f, 0.0f, 1.0f), materials[4]));
   stuff.push_back(new Plane(glm::vec3(0.0f, 0.0f, 17.0f), glm::vec3(0.0f, 0.0f, -1.0f), materials[4]));
  
   std::vector<Light> lights;
   lights.push_back(Light(glm::vec3(0.6f, 4.0f, 5.0f), glm::vec3(0.4f, 0.2f, 0.3f),1.0f));
   lights.push_back(Light(glm::vec3(3.1f, 1.9f, -6.0f), glm::vec3(0.2f, 0.4f, 0.2f),1.3f));

   float fov = glm::pi<float>() / 4.0f;
   auto timeThen = std::chrono::system_clock::now(), timeNow = std::chrono::system_clock::now();
   float elapsedTime = 0.0f;
   #pragma omp parallel for schedule(dynamic)
   for(int i = 0; i < width;  i++){
	   for(int j = 0; j < height; j++){
		   timeNow = std::chrono::system_clock::now();
		   std::chrono::duration<float> deltaChrono = timeNow - timeThen;
		   timeThen = timeNow;
		   float deltaTime = deltaChrono.count();
		
		   int currentPos = i + j * width;
		   
		   glm::mat3 rotMat = glm::rotate(glm::radians(15.0f), glm::vec3(0.0, 1.0, 0.0));
		   glm::vec3 finalResult;
		   for(int sample = 0; sample < samples; sample++){
			    float sampleX = (i + 0.5f + ((sample < 2) ? -0.25f : 0.25f)); 
                float sampleY = (j + 0.5f + ((sample >= 2) ? -0.25f : 0.25f));
				glm::vec3 dir = rotMat * glm::normalize(calculateWin(fov, sampleX, sampleY));
				Ray currentRay(glm::vec3(4.2, 0.0, 3.0), dir);
				finalResult += cast_ray(currentRay, stuff, lights);
		   }
           finalResult /= samples;
		   data[currentPos] = convertVec(finalResult);
		   
		   elapsedTime += deltaTime;
	   }
   }
   
   stbi_write_png("render.png", width, height, 3, data, 0);
   delete[] data;
   std::cout << "Total time taken to render: " << elapsedTime << std::endl;
   return 0;
}