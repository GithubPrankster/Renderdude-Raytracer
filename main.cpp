#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#define STB_IMAGE_WRITE_IMPLEMENTATION
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
	
	Vec operator* (double o){
		return Vec(x * o, y * o, z * o);
	}
	
	Vec operator/ (double o){
		return Vec(x / o, y / o, z / o);
	}
	
	bool operator< (double o){
		if(x < o && y < o && z < o) return true;
		else return false;
	}
	
	Vec operator- (Vec o){
		return Vec(x - o.x, y - o.y, z - o.z);
	}
	
	double dot(Vec o){
		return (x * o.x + y * o.y + z * o.z);
	}
	//Since
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
	
	RGB operator* (double o){
		return RGB(std::clamp(r * o, 0.0, 255.0), std::clamp(g * o, 0.0, 255.0), std::clamp(b * o, 0.0, 255.0));
	}
};

struct Material{
	RGB color;
	Material(RGB diffuse) : color(diffuse){}
	Material() = default;
};

struct Sphere{
	Vec pos;
	double radius;
	Material material;
	
	Sphere(Vec c, double r, Material mat) : pos(c), radius(r), material(mat) {}
	
	bool intersect(Vec orig, Vec dir, double t0){
		Vec L = pos - orig;
		double tca = L.dot(dir);
		double d2 = L.square() - tca * tca;
		if (d2 > radius * radius) return false;
		double thc = sqrtf(radius * radius - d2);
		t0 = tca - thc;
		double t1 = tca + thc;
		if (t0 < 0) t0 = t1;
		if (t0 < 0) return false;
		return true;
	}
	
	Vec getNormal(Vec pi){
		return (pi - pos) / radius;
	}
};

struct Light{
	Vec pos;
	double intensity;
	Light(Vec p, double i) : pos(p), intensity(i) {}
};

bool sceneIntersection(Vec orig, Vec dir, std::vector<Sphere> spheres, Vec &hitPos, Vec &normal, Material &objMat){
	double sphere_dist = std::numeric_limits<double>::max();
	for(size_t i = 0; i < spheres.size(); i++){
		double dist_i;
		if(spheres[i].intersect(orig, dir, dist_i) && dist_i < sphere_dist){
			sphere_dist = dist_i;
			hitPos = orig + dir * dist_i;
			normal = (hitPos - spheres[i].pos).normalize();
			objMat = spheres[i].material;
		}
	}
	double checkerboard_dist = std::numeric_limits<double>::max();
    if (fabs(dir.y)>1e-3)  {
        double d = -(orig.y+5)/dir.y; // the checkerboard plane has equation y = -4
        Vec pt = orig + dir*d;
        if (d>0 && fabs(pt.x)<10 && pt.z<-10 && pt.z>-30 && d<sphere_dist) {
            checkerboard_dist = d;
            hitPos = pt;
            normal = Vec(0,1,0);
            objMat.color = (int(.5*hitPos.x+1000) + int(.5*hitPos.z)) & 1 ? RGB(255,255,255) : RGB(83, 255, 50);
        }
    }
    return std::min(sphere_dist, checkerboard_dist)<1000;
}

RGB cast_ray(Vec orig, Vec dir, std::vector<Sphere> spheres, std::vector<Light> lights) {
	//Hit point, normal and obj's color are used for funny shader thingies 
    Vec hitPoint, Normal;
	Material obtMat;
	
    if (!sceneIntersection(orig, dir, spheres, hitPoint, Normal, obtMat)) {
        return RGB(10, 50, 60); // BG color!
    }
	double totalIntensity = 0.0;
	for(size_t q = 0; q < lights.size(); q++){
		Vec light_dir = (lights[q].pos - hitPoint).normalize();
		double light_dist = (lights[q].pos - hitPoint).length();
		Vec shadow_orig = (Normal * light_dist) < 0 ? hitPoint - Normal * 1e-3 : hitPoint + Normal * 1e-3;
        Vec shadow_pt, shadow_N;
        Material tmpmaterial;
        if (sceneIntersection(shadow_orig, light_dir, spheres, shadow_pt, shadow_N, tmpmaterial) && (shadow_pt-shadow_orig).length() < light_dist)
			totalIntensity  += lights[q].intensity * std::max(0.0, light_dir.dot(Normal));
		else totalIntensity = 0.5;	
	}
	RGB fullColor = obtMat.color * totalIntensity;
    return fullColor;
}

int main() {
   RGB data[width * height];
   
   Material reddy(RGB(128, 70, 70));
   Material bluey(RGB(40, 70, 128));
   Material greeny(RGB(70, 128, 40));
   
   std::vector<Sphere> spheres;
   spheres.push_back(Sphere(Vec(0, 0, -14), 2, reddy));
   spheres.push_back(Sphere(Vec(1, 1.2, -17), 2, bluey));
   spheres.push_back(Sphere(Vec(-2, 2, -5), 1.2, greeny));
   spheres.push_back(Sphere(Vec(1.2, -2, -10), 1.8, reddy));
   
   std::vector<Light> lights;
   lights.push_back(Light(Vec(-1, 2, -7), 1.0));
   
   double fov = 3.141596 / 4.0;

   for(int i = 0; i < width;  i++){
	   for(int j = 0; j < height; j++){
		   int currentPos = i + j * width;
		   double x =  (2*(i + 0.5)/(double)width  - 1)*tan(fov/2.)*width/(double)height;
           double y = -(2*(j + 0.5)/(double)height - 1)*tan(fov/2.);
           Vec dir = Vec(x, y, -1).normalize();
		   data[currentPos] = cast_ray(Vec(0, 0.8, 0), dir, spheres, lights);
	   }
   }
   stbi_write_png("woah.png", width, height, 3, &data, 0);
   return 0;
}

