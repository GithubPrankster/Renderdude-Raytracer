#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <chrono>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include "stb_image_write.h"

const int width = 640, height = 480;

//The vector class, a joint effort between brain and cube.
struct Vec{
	double x = 0.0, y = 0.0, z = 0.0;
	
	Vec(double q, double w, double e) : x(q), y(w), z(e){}
	Vec() = default;	
	
	Vec operator+ (Vec o){
		return Vec(x + o.x, y + o.y, z + o.z);
	}
	
	Vec operator+ (double o){
		return Vec(x + o, y + o, z + o);
	}
	
	Vec operator* (double o){
		return Vec(x * o, y * o, z * o);
	}
	
	Vec operator- (Vec o){
		return Vec(x - o.x, y - o.y, z - o.z);
	}
	
	double dot(Vec o){
		return (x * o.x + y * o.y + z * o.z);
	}
	
	double square(){
		Vec self(x, y, z);
		return self.dot(self);
	}
	
	double length(){
		Vec self(x, y, z);
		//Sorry for the sqrtf if you're into the quick one. May implement that latter? IDK!
		return sqrtf(self.square());
	}
	
	Vec normalize(){
		Vec self(x, y, z);
		return Vec(x * (1.0 / self.length()), y * (1.0 / self.length()), z * (1.0 / self.length()));
	};
};

struct RGB{
	unsigned char r = 0, g = 0, b = 0;
	
	RGB(unsigned char c, unsigned char y, unsigned char m) : r(c), g(y), b(m){}
	RGB() = default;
	
};

struct Material{
	Vec color;
	Material(Vec diffuse) : color(diffuse){}
	Material() = default;
};

struct Light{
	Vec pos;
	double intensity;
	Light(Vec p, double i) : pos(p), intensity(i) {}
	Light() = default;
};

struct Sphere{
	Vec pos;
	double radius;
	Material material;
	
	Sphere(Vec c, double r, Material mat) : pos(c), radius(r), material(mat) {}
	
	bool intersect(Vec orig, Vec dir, double &t2){
		double radius2 = radius * radius;
		Vec L = pos - orig;

		double tca = L.dot(dir);
		double d2 = L.square() - tca * tca;

		if (d2 > radius2) return false;

		double thc = sqrtf(radius2 - d2);

		double t0 = tca - thc;
		double t1 = tca + thc;

		if (t0 > t1) std::swap(t0, t1);
		if (t0 < 0) {
			t0 = t1;
			if(t0 < 0) return false;
		}
		t2 = t0;
		return true;
	}
};

bool sceneIntersection(Vec orig, Vec dir, std::vector<Sphere> spheres, Material &objMat, Vec &hitPoint, Vec &normal){
	double sphere_dist = std::numeric_limits<double>::max();
	for(size_t i = 0; i < spheres.size(); i++){
		double dist_i = 0.0;
		if(spheres[i].intersect(orig, dir, dist_i) && dist_i < sphere_dist){
			sphere_dist = dist_i;
			hitPoint = orig + dir * dist_i;
			normal = (hitPoint - spheres[i].pos).normalize();
			objMat = spheres[i].material;
		}
	}
    return sphere_dist<1000;
}

Vec cast_ray(Vec orig, Vec dir, std::vector<Sphere> spheres, std::vector<Light> lights) {
	Vec hitPoint = Vec(), normal = Vec();
	Material obtMat;
	
    if (!sceneIntersection(orig, dir, spheres, obtMat, hitPoint, normal)) {
        return Vec(0.5, 0.6, 5.0); // BG color!
    }
	double totalDt = 0.0;
	for(size_t i = 0; i < lights.size(); i++){
		Vec L = (lights[i].pos - hitPoint).normalize();
		double lightDist = (lights[i].pos - hitPoint).length();
		
		Vec shadow_orig = L.dot(normal) < 0 ? hitPoint - normal * 1e-3 : hitPoint + normal * 1e-3;
        Vec shadow_pt, shadow_N;
        Material tmpmaterial;
        if (sceneIntersection(shadow_orig, L, spheres, tmpmaterial, shadow_pt, shadow_N) && (shadow_pt-shadow_orig).length() < lightDist)
            continue;
		
		totalDt += lights[i].intensity * std::max(0.0, L.dot(normal));
	}
	
	return obtMat.color * totalDt;
}

RGB convertVec(Vec d){
	return RGB(std::round(d.x * 255.0), std::round(d.y * 255.0), std::round(d.z * 255.0));
}

int main() {
   RGB data[width * height];
   
   Material reddy(Vec(0.5, 0.2, 0.3));
   Material bluey(Vec(0.2, 0.3, 0.5));
   Material greeny(Vec(0.3, 0.5, 0.2));
   
   std::vector<Sphere> spheres;
   spheres.push_back(Sphere(Vec(0, 0, -14), 2, reddy));
   spheres.push_back(Sphere(Vec(1, 1.2, -17), 2, bluey));
   spheres.push_back(Sphere(Vec(-2, 2, -5), 1.2, greeny));
   spheres.push_back(Sphere(Vec(3, -3, -10), 2.1, greeny));
   
   std::vector<Light> lights;
   lights.push_back(Light(Vec(0, 0, -5), 1.4));

   double fov = 3.141596 / 4.0;
   auto timeThen = std::chrono::system_clock::now(), timeNow = std::chrono::system_clock::now();
   float elapsedTime = 0.0f;

   for(int i = 0; i < width;  i++){
	   for(int j = 0; j < height; j++){
		   timeNow = std::chrono::system_clock::now();
		   std::chrono::duration<float> deltaChrono = timeNow - timeThen;
		   timeThen = timeNow;
		   float deltaTime = deltaChrono.count();
		
		   int currentPos = i + j * width;
		   double x =  (2*(i + 0.5)/(double)width  - 1)*tan(fov/2.)*width/(double)height;
           double y = -(2*(j + 0.5)/(double)height - 1)*tan(fov/2.);
           Vec dir = Vec(x, y, -1).normalize();
		   data[currentPos] = convertVec(cast_ray(Vec(), dir, spheres, lights));
		   
		   elapsedTime += deltaTime;
	   }
   }
   stbi_write_png("render.png", width, height, 3, &data, 0);
   std::cout << elapsedTime << std::endl;
   return 0;
}