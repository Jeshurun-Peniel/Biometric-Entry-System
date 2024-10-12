// students.h

#ifndef STUDENTS_H
#define STUDENTS_H

#include <Arduino.h>

struct Student {
  String name;
  String reg;
  String rfidID;
  uint8_t fingerprintID;
};

// Declare the students array with initial values
extern Student students[42];

#endif // STUDENTS_H
