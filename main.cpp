#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#include <random>
#include <omp.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "stb_image_write.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

const int width = 880, height = 720;

std::random_device device;
std::mt19937 generator(device());
std::uniform_real_distribution<float> distribution(-0.8, 0.5);

struct RGB{
	unsigned char r = 0, g = 0, b = 0;
	
	RGB(unsigned char c, unsigned char y, unsigned char m) : r(c), g(y), b(m){}
	RGB() = default;
	
};

enum MaterialType{
	Diffuse, Specular, Reflective, Emissive
};

struct Material{
	glm::vec3 pbrCtrl = glm::vec3(1.0, 1.0, 0.0);
	glm::vec3 color;
	float specualirity;
	MaterialType type;
	Material(glm::vec3 throttle, glm::vec3 diff, float specular, MaterialType t) : pbrCtrl(throttle), color(diff), specualirity(specular), type(t) {}
	Material() = default;
};

struct Light{
	glm::vec3 pos;
	glm::vec3 color;
	float intensity;
	Light(glm::vec3 p, glm::vec3 c,float i) : pos(p), color(c),intensity(i) {}
	Light() = default;
};

struct Sphere{
	glm::vec3 pos;
	float radius;
	Material material;
	
	Sphere(glm::vec3 c, float r, Material mat) : pos(c), radius(r), material(mat) {}
	
	bool intersect(glm::vec3 orig, glm::vec3 dir, float &t2){
		float radius2 = radius * radius;
		glm::vec3 L = pos - orig;

		float tca = glm::dot(L,dir);
		float d2 = glm::dot(L, L) - tca * tca;

		if (d2 > radius2) return false;

		float thc = sqrtf(radius2 - d2);

		float t0 = tca - thc;
		float t1 = tca + thc;

		if (t0 > t1) std::swap(t0, t1);
		if (t0 < 0) {
			t0 = t1;
			if(t0 < 0) return false;
		}
		t2 = t0;
		return true;
	}
};

bool sceneIntersection(glm::vec3 orig, glm::vec3 dir, std::vector<Sphere> spheres, Material &objMat, glm::vec3 &hitPoint, glm::vec3 &normal){
	float sphere_dist = std::numeric_limits<float>::max();
	for(size_t i = 0; i < spheres.size(); i++){
		float dist_i = 0.0f;
		if(spheres[i].intersect(orig, dir, dist_i) && dist_i < sphere_dist){
			sphere_dist = dist_i;
			hitPoint = orig + dir * dist_i;
			normal = glm::normalize(hitPoint - spheres[i].pos);
			objMat = spheres[i].material;
		}
	}
    return sphere_dist<1000;
}

glm::vec3 clampRay(glm::vec3 col){
	glm::vec3 res = col;
	res.x = col.x < 0 ? 0 : col.x > 1 ? 1 : col.x;
	res.y = col.y < 0 ? 0 : col.y > 1 ? 1 : col.y;
	res.z = col.z < 0 ? 0 : col.z > 1 ? 1 : col.z;
	return res;
}

glm::vec3 cast_ray(glm::vec3 orig, glm::vec3 dir, std::vector<Sphere> spheres, std::vector<Light> lights, size_t depth = 0) {
	glm::vec3 hitPoint = glm::vec3(), normal = glm::vec3();
	Material obtMat;
	
    if (depth > 8 || !sceneIntersection(orig, dir, spheres, obtMat, hitPoint, normal)) {
        return glm::vec3(0.5f, 0.6f, 0.7f); // BG color!
    }
	
	glm::vec3 reflect_dir = glm::normalize(glm::reflect(dir, normal));
	glm::vec3 refract_dir = glm::normalize(glm::refract(dir, normal, 1.33f));
    glm::vec3 reflect_orig = glm::dot(reflect_dir, normal) < 0 ? hitPoint - normal * 1e-3f : hitPoint + normal * 1e-3f;
	glm::vec3 refract_orig = glm::dot(refract_dir, normal) < 0 ? hitPoint - normal * 1e-3f : hitPoint + normal * 1e-3f;
    glm::vec3 reflect_color = cast_ray(reflect_orig, reflect_dir, spheres, lights, depth + 1);
	glm::vec3 refract_color = cast_ray(refract_orig, refract_dir, spheres, lights, depth + 1);
	
	float totalDt = 0.0f, totalSpecular = 0.0f;
	for(size_t i = 0; i < lights.size(); i++){
		glm::vec3 L = glm::normalize(lights[i].pos - hitPoint);
		float lightDist = glm::length(lights[i].pos - hitPoint);
		
		glm::vec3 shadow_orig = glm::dot(L,normal) < 0 ? hitPoint - normal * 1e-3f : hitPoint + normal * 1e-3f;
        glm::vec3 shadow_pt, shadow_N;
        Material tmpmaterial;
        if (sceneIntersection(shadow_orig, L, spheres, tmpmaterial, shadow_pt, shadow_N) && glm::length(shadow_pt-shadow_orig) < lightDist){
			totalDt += (lights[i].intensity * std::max(0.f, glm::dot(L, normal))) - 0.3;
			totalSpecular += (powf(std::max(0.0f, glm::dot(-glm::reflect(-L, normal), dir)),obtMat.specualirity) * lights[i].intensity - distribution(generator)) - 0.4;   
		}else{
			totalDt += lights[i].intensity * std::max(0.f, glm::dot(L, normal));
			totalSpecular += powf(std::max(0.0f, glm::dot(-glm::reflect(-L, normal), dir)),obtMat.specualirity) * lights[i].intensity - distribution(generator);
		}	
	}
	return clampRay(obtMat.type == Reflective ? reflect_color * totalDt * obtMat.pbrCtrl.z + refract_color: 
	obtMat.color * totalDt * obtMat.pbrCtrl.x + glm::vec3(1.0f) * 
	std::floor(totalSpecular) * obtMat.pbrCtrl.y);
}

RGB convertVec(glm::vec3 d){
	return RGB(std::round(d.x * 255.0f), std::round(d.y * 255.0f), std::round(d.z * 255.0f));
}

int main() {
   RGB data[width * height];
   
   Material reddy(glm::vec3(0.9, 0.3, 0.1), glm::vec3(0.5f, 0.2f, 0.3f), 1.2f, Diffuse);
   Material bluey(glm::vec3(0.95, 0.8, 0.9), glm::vec3(0.2f, 0.3f, 0.5f), 6.0f, Reflective);
   Material greeny(glm::vec3(0.6, 0.1, 0.2), glm::vec3(0.3f, 0.5f, 0.2f), 1.0f, Diffuse);
   
   std::vector<Sphere> spheres;
   spheres.push_back(Sphere(glm::vec3(0.0f, 0.0f, -14.0f), 2.0f, reddy));
   spheres.push_back(Sphere(glm::vec3(1.0f, -1.2f, -17.0f), 2.0f, bluey));
   
   spheres.push_back(Sphere(glm::vec3(-2.0f, 2.0f, -5.0f), 1.2f, greeny));
   spheres.push_back(Sphere(glm::vec3(2.0f, -2.0f, -5.0f), 1.2f, greeny));
   
   spheres.push_back(Sphere(glm::vec3(2.0f, 2.0f, -5.0f), 1.2f, greeny));
   spheres.push_back(Sphere(glm::vec3(-2.0f, -2.0f, -5.0f), 1.2f, greeny));
   
   spheres.push_back(Sphere(glm::vec3(2.2f, -3.0f, -16.0f), 3.0f, bluey));
   spheres.push_back(Sphere(glm::vec3(-2.2f, -3.0f, -16.0f), 3.0f, bluey));
   spheres.push_back(Sphere(glm::vec3(2.2f, 3.0f, -16.0f), 3.0f, bluey));
   spheres.push_back(Sphere(glm::vec3(-2.2f, 3.0f, -16.0f), 3.0f, bluey));
   
   std::vector<Light> lights;
   lights.push_back(Light(glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.2f, 0.5f, 0.3f),1.4f));

   float fov = 3.141596f / 4.0f;
   auto timeThen = std::chrono::system_clock::now(), timeNow = std::chrono::system_clock::now();
   float elapsedTime = 0.0f;

   #pragma omp parallel for
   for(int i = 0; i < width;  i++){
	   for(int j = 0; j < height; j++){
		   timeNow = std::chrono::system_clock::now();
		   std::chrono::duration<float> deltaChrono = timeNow - timeThen;
		   timeThen = timeNow;
		   float deltaTime = deltaChrono.count();
		
		   int currentPos = i + j * width;
		   float x =  (2*(i + 0.5f)/(float)width  - 1)*tan(fov/2.0f)*width/(float)height;
           float y = -(2*(j + 0.5f)/(float)height - 1)*tan(fov/2.0f);
           glm::vec3 dir = glm::normalize(glm::vec3(x, y, -1));
		   data[currentPos] = convertVec(cast_ray(glm::vec3(), dir, spheres, lights));
		   
		   elapsedTime += deltaTime;
	   }
   }
   stbi_write_png("render.png", width, height, 3, &data, 0);
   std::cout << elapsedTime << std::endl;
   return 0;
}