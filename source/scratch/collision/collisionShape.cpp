#include "collisionShape.hpp"
#include "../math.hpp"
#include <cmath>
CollisionShape::CollisionShape() {
    float radius = 50;
    for (int i = 0; i < COLLISION_POINT_COUNT; i++) {
        float angle = i * 2 * M_PI / COLLISION_POINT_COUNT;
        originalPoints[i].x = cos(angle) * radius;
        originalPoints[i].y = sin(angle) * radius;
    }
    for (int i = 0; i < COLLISION_POINT_COUNT; ++i) {
        points[i] = originalPoints[i];
    }
}
CollisionShape::CollisionShape(float radius) {
    for (int i = 0; i < COLLISION_POINT_COUNT; i++) {
        float angle = i * 2 * M_PI / COLLISION_POINT_COUNT;
        originalPoints[i].x = cos(angle) * radius;
        originalPoints[i].y = sin(angle) * radius;
    }
    for (int i = 0; i < COLLISION_POINT_COUNT; ++i) {
        points[i] = originalPoints[i];
    }
}
bool CollisionShape::CheckCollision(CollisionShape otherShape) {
    bool hit = false;
    for (int i = 0; i < COLLISION_POINT_COUNT; i++) {
        int hitCount = 0;
        float pointX = points[i].x;
        float pointY = points[i].y;

        for (int d = 0; d < COLLISION_POINT_COUNT; d++) {
            CollisionPoint point1 = otherShape.points[d];
            int d2 = d + 1;
            if (d2 >= COLLISION_POINT_COUNT) d2 = 0;
            CollisionPoint point2 = otherShape.points[d2];
            float upper = std::min(point1.y, point2.y);
            float lower = std::max(point1.y, point2.y);
            if (upper < pointY && pointY <= lower) {

                float lineSlope = (point1.y - point2.y) / (point1.x - point2.x);
                if (lineSlope != 0) {
                    if ((point1.y > pointY) != (point2.y > pointY)) {
                        float intersectX = (point2.x - point1.x) * (pointY - point1.y) / (point2.y - point1.y) + point1.x;
                        if (intersectX > pointX) hitCount++;
                    }
                }
            }
        }
        hit = hit || (hitCount % 2 == 1);
    }
    return hit;
}
void CollisionShape::SetCollision(float newX, float newY, float newAngle, float scale) {
    for (int i = 0; i < COLLISION_POINT_COUNT; ++i) {
        points[i] = originalPoints[i];
        if (newAngle != angle) {
            float pointAngle = atan2(points[i].y, points[i].x);
            float distance = sqrt(pow(points[i].x, 2) + pow(points[i].y, 2));
            points[i].x = cos(pointAngle + newAngle) * distance;
            points[i].y = sin(pointAngle + newAngle) * distance;
        }
        points[i].x *= scale;
        points[i].y *= scale;
        points[i].x += newX;
        points[i].y += newY;
    }
    angle = newAngle;
}