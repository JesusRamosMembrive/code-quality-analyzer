#pragma once

#include <cstddef>

class Shape
{
public:
    double area() const;
    double perimeter() const;
    void scale(double factor);

private:
    double size_;
};
