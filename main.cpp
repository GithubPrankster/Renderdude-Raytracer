#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#include <random>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "stb_image_write.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

const int width = 880, height = 720;

//std::random_device device;
//std::mt19937 generator(device());
//std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
//Rand unit sphere will be used for random direction stuff and whatnots
//glm::vec3 randUnitSphere(){
//	glm::vec3 point;
//	do{
//		point = 2.0f * glm::vec3(distribution(generator)) - glm::vec3(1.0f);
//	}while(pow(glm::length(point), 2.0f) >= 1.0);
//	return point;
//}

struct RGB{
	unsigned char r = 0, g = 0, b = 0;
	
	RGB(unsigned char c, unsigned char y, unsigned char m) : r(c), g(y), b(m){}
	RGB() = default;
	
};

//Soon materials will have more meaning. Right now only Diffuse and Reflective are used.
enum MaterialType{
	Diffuse, Specular, Reflective, Emissive
};

struct Ray{
	glm::vec3 orig, dir;
	Ray(glm::vec3 o, glm::vec3 d) : orig(o), dir(d) {}
};

struct Material{
	glm::vec3 pbrCtrl = glm::vec3(1.0, 1.0, 0.0);
	glm::vec3 color;
	float specualirity;
	MaterialType type;
	Material(glm::vec3 throttle, glm::vec3 diff, float specular, MaterialType t) : pbrCtrl(throttle), color(diff), specualirity(specular), type(t) {}
	Material() = default;
};

struct hitHistory{
	float dist;
	glm::vec3 hitPoint, normal;
	Material *obtMat;
	hitHistory(float d, glm::vec3 hP, glm::vec3 n, Material &oM) : dist(d), hitPoint(hP), normal(n), obtMat(&oM) {}
	hitHistory() = default;
};

struct Light{
	glm::vec3 pos;
	//Color is not used yet.
	glm::vec3 color;
	float intensity;
	Light(glm::vec3 p, glm::vec3 c,float i) : pos(p), color(c),intensity(i) {}
	Light() = default;
};

struct Object{
	glm::vec3 pos;
	Material material;
	Object(glm::vec3 p, Material mat) : pos(p), material(mat) {}
	Object() = default;
	virtual bool intersect(Ray ray, float &dist) = 0;
	virtual glm::vec3 getNormal(Ray ray, float dist) = 0;
};

struct Sphere : Object{
	float radius;
	
	Sphere (glm::vec3 c, float r, Material mat) : Object(c, mat), radius(r)  {}
	
	bool intersect(Ray ray, float &t2){
		float radius2 = radius * radius;
		glm::vec3 L = pos - ray.orig;

		float tca = glm::dot(L,ray.dir);
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
	
	glm::vec3 getNormal(Ray ray, float dist){
		glm::vec3 hitPoint = ray.orig + ray.dir * dist;
		return glm::normalize(hitPoint - pos);
	}
};

struct Plane : Object{
	glm::vec3 normal;
	Plane(glm::vec3 p, glm::vec3 n, Material mat) : Object(p, mat), normal(n) {}
	
	bool intersect(Ray ray, float &dist){
		float denom = glm::dot(normal, ray.dir);

		if(abs(denom) > 1e-6f){
			dist = glm::dot(pos - ray.orig, normal) / denom;
			return (dist >= 1e-6f);
		}
		return false;
	}
	glm::vec3 getNormal(Ray ray, float dist){
		return normal;
	}
};

bool sceneIntersection(Ray ray, std::vector<Object*> stuff, hitHistory &history){
	float stuff_dist = std::numeric_limits<float>::max();
	for(auto &object : stuff){
		float dist_i = 0.0f;
		if(object->intersect(ray, dist_i) && dist_i < stuff_dist){
			stuff_dist = dist_i;
			hitHistory gotHist(dist_i, ray.orig + ray.dir * dist_i, object->getNormal(ray, dist_i), object->material);
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
	hitHistory rayHistory;
    if (depth > 8 || !sceneIntersection(ray, stuff, rayHistory)) {
        return glm::vec3(0.0f, 0.0f, 0.0f); // BG color!
    }
	
	glm::vec3 reflect_dir = glm::normalize(glm::reflect(ray.dir, rayHistory.normal));
    glm::vec3 reflect_orig = glm::dot(reflect_dir, rayHistory.normal) < 0 ? rayHistory.hitPoint - rayHistory.normal * 1e-3f : rayHistory.hitPoint + rayHistory.normal * 1e-3f;
    glm::vec3 reflect_color = cast_ray(Ray(reflect_orig, reflect_dir), stuff, lights, depth + 1);
	
	float totalDt = 0.0f, totalSpecular = 0.0f;
	for(size_t i = 0; i < lights.size(); i++){
		glm::vec3 L = glm::normalize(lights[i].pos - rayHistory.hitPoint);
		float lightDist = glm::length(lights[i].pos - rayHistory.hitPoint);
		
		Ray shadowRay(glm::dot(L,rayHistory.normal) < 0 ? rayHistory.hitPoint - rayHistory.normal * 1e-3f : rayHistory.hitPoint + rayHistory.normal * 1e-3f, L);
		hitHistory shadowHist;
		
        if (sceneIntersection(shadowRay, stuff, shadowHist) && glm::length(shadowHist.hitPoint - shadowRay.orig) < lightDist){
			totalDt += (lights[i].intensity * std::max(0.f, glm::dot(L, rayHistory.normal))) - 0.3;
			totalSpecular += (powf(std::max(0.0f, glm::dot(-glm::reflect(-L, rayHistory.normal), ray.dir)),rayHistory.obtMat->specualirity) * lights[i].intensity) - 0.4;  
		}
		else{
			totalDt += lights[i].intensity * std::max(0.f, glm::dot(L, rayHistory.normal));
			totalSpecular += powf(std::max(0.0f, glm::dot(-glm::reflect(-L, rayHistory.normal), ray.dir)),rayHistory.obtMat->specualirity) * lights[i].intensity;
		}	
	}
	return clampRay(rayHistory.obtMat->type == Reflective ? reflect_color * totalDt * rayHistory.obtMat->pbrCtrl.z: 
	rayHistory.obtMat->color * totalDt * rayHistory.obtMat->pbrCtrl.x + glm::vec3(1.0f) * 
	std::floor(totalSpecular) * rayHistory.obtMat->pbrCtrl.y);
}

RGB convertVec(glm::vec3 d){
	return RGB(std::round(d.x * 255.0f), std::round(d.y * 255.0f), std::round(d.z * 255.0f));
}

int main() {
   RGB data[width * height];
   
   Material reddy(glm::vec3(0.9, 0.3, 0.1), glm::vec3(0.5f, 0.2f, 0.3f), 1.2f, Diffuse);
   Material bluey(glm::vec3(0.95, 0.8, 0.9), glm::vec3(0.2f, 0.3f, 0.5f), 6.0f, Reflective);
   Material greeny(glm::vec3(0.6, 0.1, 0.2), glm::vec3(0.3f, 0.5f, 0.2f), 1.0f, Diffuse);
   Material thingy(glm::vec3(0.6, 0.5, 0.1), glm::vec3(0.5f, 0.4f, 0.6f), 1.5f, Diffuse);
   
   std::vector<Object*> stuff;
   stuff.push_back(new Sphere(glm::vec3(0.0f, 0.0f, -14.0f), 2.0f, reddy));
   
   stuff.push_back(new Sphere(glm::vec3(-2.0f, 2.0f, -15.0f), 1.2f, bluey));
   stuff.push_back(new Sphere(glm::vec3(2.0f, -2.0f, -15.0f), 1.2f, bluey));
   
   stuff.push_back(new Sphere(glm::vec3(2.0f, 2.0f, -15.0f), 1.2f, bluey));
   stuff.push_back(new Sphere(glm::vec3(-2.0f, -2.0f, -15.0f), 1.2f, bluey));
   
   stuff.push_back(new Plane(glm::vec3(0.0f, -4.0f, -5.0f), glm::vec3(0.0f, 1.0f, 0.0f), reddy));
   stuff.push_back(new Plane(glm::vec3(0.0f, 6.0f, -5.0f), glm::vec3(0.0f, -1.0f, 0.0f), greeny));
   
   stuff.push_back(new Plane(glm::vec3(5.0f, 0.0f, -5.0f), glm::vec3(-1.0f, 0.0f, 0.0f), bluey));
   stuff.push_back(new Plane(glm::vec3(-5.0f, 0.0f, -5.0f), glm::vec3(1.0f, 0.0f, 0.0f), bluey));
   
   stuff.push_back(new Plane(glm::vec3(0.0f, 0.0f, -17.0f), glm::vec3(0.0f, 0.0f, 1.0f), thingy));
   stuff.push_back(new Plane(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, -1.0f), thingy));
  
   
   std::vector<Light> lights;
   lights.push_back(Light(glm::vec3(0.6f, 0.6f, -5.0f), glm::vec3(0.2f, 0.5f, 0.3f),1.4f));

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
		   Ray currentRay(glm::vec3(0.0, 1.0, 0.0), dir);
		   data[currentPos] = convertVec(cast_ray(currentRay, stuff, lights));
		   
		   elapsedTime += deltaTime;
	   }
   }
   
   stbi_write_png("render.png", width, height, 3, &data, 0);
   std::cout << elapsedTime << std::endl;
   return 0;
}