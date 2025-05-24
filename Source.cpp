#include <GL/freeglut.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ctime>

const int WIDTH = 800;
const int HEIGHT = 600;
const float PI = 3.141592653589793f;

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
    float length() const { return sqrt(x * x + y * y + z * z); }
    Vec3 normalize() const { float l = length(); return { x / l, y / l, z / l }; }
    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }
};

struct Sphere {
    Vec3 center;
    float radius;
    Vec3 color;
    float reflectivity; // 0 - матовый, 1 - зеркальный
    float transparency; // 0 - непрозрачный, 1 - полностью прозрачный
    float refractiveIndex; // коэффициент преломления
    Vec3 originalCenter; // для анимации
};

struct Ray {
    Vec3 origin, direction;
    Vec3 pointAt(float t) const { return origin + direction * t; }
};

struct Light {
    Vec3 position;
    Vec3 color;
    float intensity;
};

std::vector<Sphere> spheres = {
    {{0, 0, -5}, 1.0f, {1.0f, 0.2f, 0.2f}, 0.0f, 0.0f, 1.0f, {0, 0, -5}}, // Матовая красная
    {{2, 0, -5}, 1.0f, {0.2f, 0.2f, 0.2f}, 0.9f, 0.0f, 1.0f, {2, 0, -5}},  // Зеркальная
    {{-2, 0, -5}, 1.0f, {0.9f, 0.9f, 0.9f}, 0.1f, 0.9f, 1.5f, {-2, 0, -5}} // Стеклянная
};

Light light = { {5, 5, -3}, {1, 1, 1}, 1.5f };
float timeValue = 0.0f;

bool intersectSphere(const Ray& ray, const Sphere& sphere, float& t) {
    Vec3 oc = ray.origin - sphere.center;
    float a = ray.direction.dot(ray.direction);
    float b = 2.0f * oc.dot(ray.direction);
    float c = oc.dot(oc) - sphere.radius * sphere.radius;
    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    t = (-b - sqrt(discr)) / (2.0f * a);
    return t > 0;
}

Vec3 computeLighting(const Vec3& point, const Vec3& normal, const Vec3& color) {
    Vec3 lightDir = (light.position - point).normalize();
    float diffuse = std::max(0.0f, normal.dot(lightDir)) * light.intensity;

    // Тени
    Ray shadowRay = { point + normal * 0.001f, lightDir };
    float t;
    for (const auto& sphere : spheres) {
        if (intersectSphere(shadowRay, sphere, t)) {
            diffuse *= 0.2f; // Ослабление света в тени
            break;
        }
    }

    return color * diffuse;
}

Vec3 reflect(const Vec3& incident, const Vec3& normal) {
    return incident - normal * 2.0f * incident.dot(normal);
}

bool refract(const Vec3& incident, const Vec3& normal, float n1, float n2, Vec3& refracted) {
    Vec3 norm = normal;
    float n = n1 / n2;
    float cosI = -incident.dot(norm);
    float sinT2 = n * n * (1.0f - cosI * cosI);

    if (sinT2 > 1.0f) return false; // Полное внутреннее отражение

    float cosT = sqrt(1.0f - sinT2);
    refracted = incident * n + norm * (n * cosI - cosT);
    return true;
}

Vec3 traceRay(const Ray& ray, int depth = 0) {
    if (depth > 5) return { 0, 0, 0 }; // Ограничение рекурсии

    float tMin = INFINITY;
    const Sphere* hitSphere = nullptr;

    for (const auto& sphere : spheres) {
        float t;
        if (intersectSphere(ray, sphere, t) && t < tMin) {
            tMin = t;
            hitSphere = &sphere;
        }
    }

    if (!hitSphere) return { 0.1f, 0.1f, 0.1f }; // Фон

    Vec3 hitPoint = ray.pointAt(tMin);
    Vec3 normal = (hitPoint - hitSphere->center).normalize();
    Vec3 color = computeLighting(hitPoint, normal, hitSphere->color);

    // Отражение для зеркальных и стеклянных объектов
    if (hitSphere->reflectivity > 0 || hitSphere->transparency > 0) {
        Vec3 reflectColor = { 0, 0, 0 };
        Vec3 refractColor = { 0, 0, 0 };

        // Вычисление отраженного луча
            Vec3 reflectDir = reflect(ray.direction, normal).normalize();
        Ray reflectRay = { hitPoint + normal * 0.001f, reflectDir };
        reflectColor = traceRay(reflectRay, depth + 1);

        // Для прозрачных объектов вычисляем преломление
        if (hitSphere->transparency > 0) {
            Vec3 refractDir;
            bool inside = (ray.direction.dot(normal) > 0);
            Vec3 norm = inside ? normal * -1.0f : normal;
            float n1 = inside ? hitSphere->refractiveIndex : 1.0f;
            float n2 = inside ? 1.0f : hitSphere->refractiveIndex;

            if (refract(ray.direction, norm, n1, n2, refractDir)) {
                Ray refractRay = { hitPoint - norm * 0.001f, refractDir.normalize() };
                refractColor = traceRay(refractRay, depth + 1);
            }

            // Смешиваем отражение и преломление по Френелю (упрощенная версия)
            float fresnel = 0.1f + 0.9f * pow(1.0f - fabs(ray.direction.dot(normal)), 5.0f);
            color = color * (1 - hitSphere->transparency) +
                (reflectColor * fresnel + refractColor * (1 - fresnel)) * hitSphere->transparency;
        }
        else {
            // Только отражение для зеркальных объектов
            color = color * (1 - hitSphere->reflectivity) + reflectColor * hitSphere->reflectivity;
        }
    }

    return color;
}

void updateScene() {
    timeValue = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    // Анимация сфер
    for (size_t i = 0; i < spheres.size(); ++i) {
        float t = timeValue * (i + 1) * 0.5f;
        spheres[i].center.y = spheres[i].originalCenter.y + sin(t) * 0.5f;
        spheres[i].center.x = spheres[i].originalCenter.x + cos(t) * 0.5f;
    }

    // Анимация света
    light.position.x = 5.0f * cos(timeValue * 0.5f);
    light.position.y = 5.0f * sin(timeValue * 0.5f);

    glutPostRedisplay();
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_POINTS);

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            float nx = (2.0f * x) / WIDTH - 1.0f;
            float ny = 1.0f - (2.0f * y) / HEIGHT;

            Ray ray = { {0, 0, 0}, Vec3{nx, ny, -1}. normalize() };
            Vec3 color = traceRay(ray);

            // Гамма-коррекция
            color.x = pow(color.x, 1.0f / 2.2f);
            color.y = pow(color.y, 1.0f / 2.2f);
            color.z = pow(color.z, 1.0f / 2.2f);

            glColor3f(color.x, color.y, color.z);
            glVertex2i(x, y);
        }
    }

    glEnd();
    glutSwapBuffers();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Advanced Ray Tracing with Motion");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);

    glutDisplayFunc(render);
    glutIdleFunc(updateScene);
    glutMainLoop();
    return 0;
}



