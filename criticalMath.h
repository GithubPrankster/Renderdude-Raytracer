struct RGB{
	unsigned char r = 0, g = 0, b = 0;
	
	RGB(unsigned char c, unsigned char y, unsigned char m) : r(c), g(y), b(m){}
	RGB() = default;
	
};

struct Light{
	glm::vec3 pos;
	glm::vec3 color;
	float intensity;
	Light(glm::vec3 p, glm::vec3 c,float i) : pos(p), color(c),intensity(i) {}
	Light() = default;
};

struct Ray{
	glm::vec3 orig, dir;
	Ray(glm::vec3 o, glm::vec3 d) : orig(o), dir(d) {}
};

enum MaterialType{
	Diffuse, Specular, Reflective, Checkered, SphereCheckered, Textured
};


struct Material{
	glm::vec3 pbrCtrl = glm::vec3(1.0, 1.0, 0.0);
	glm::vec3 color;
	float specualirity;
	MaterialType type;
	std::string name;
	
	Material(glm::vec3 throttle, glm::vec3 diff, float specular, MaterialType t) : pbrCtrl(throttle), color(diff), specualirity(specular), type(t) {}
	Material() = default;
	
	glm::vec3 returnCheckered(glm::vec3 hit){
		return (int(.8*hit.x+1000) + int(.8*hit.z)) & 1 ? glm::vec3(0.1f) : color;
	}
	
	glm::vec3 returnSphereCheckered(glm::vec3 normal){
		glm::vec2 uv = glm::vec2(glm::atan(normal.x, normal.z) / (2.0f * glm::pi<float>()) + 0.5f, glm::asin(normal.y) / glm::pi<float>() + 0.5f); 
		
		return (int)(floor(16.0f * uv.x) + floor(10.0f * uv.y)) % 2 ? glm::vec3(0.9f) : color;
	}
	void setString(std::string neam){
		name = neam;
	}
};

struct hitHistory{
	float dist;
	glm::vec3 hitPoint, normal;
	Material *obtMat;
	hitHistory(float d, glm::vec3 hP, glm::vec3 n, Material &oM) : dist(d), hitPoint(hP), normal(n), obtMat(&oM) {}
	hitHistory() = default;
};

struct Object{
	glm::vec3 pos;
	Material material;
	Object(glm::vec3 p, Material mat) : pos(p), material(mat) {}
	Object() = default;
	virtual bool intersect(Ray ray, float &dist) = 0;
	virtual glm::vec3 getNormal(glm::vec3 hitPoint) = 0;
};

struct Sphere : Object{
	float radius;
	
	Sphere (glm::vec3 c, float r, Material mat) : Object(c, mat), radius(r)  {}
	
	bool intersect(Ray ray, float &t2){
		float radius2 = radius * radius;
		glm::vec3 L = pos - ray.orig;

		float tca = glm::dot(L,ray.dir), d2 = glm::dot(L, L) - tca * tca;

		if (d2 > radius2) return false;

		float thc = sqrtf(radius2 - d2);

		float t0 = tca - thc, t1 = tca + thc;

		if (t0 > t1) std::swap(t0, t1);
		if (t0 < 0) {
			t0 = t1;
			if(t0 < 0) return false;
		}
		t2 = t0;
		return true;
	}
	
	glm::vec3 getNormal(glm::vec3 hitPoint){
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
	glm::vec3 getNormal(glm::vec3 hitPoint){
		return normal;
	}
};