#pragma once

typedef struct CharBufferInternals {
  int max;
  int length;
  char* buffer;
} CharBufferInternals;

typedef struct CharBufferInternals* CharBuffer;

CharBuffer CharBuffer_Create(int max) {
  CharBuffer self = calloc(1, sizeof(CharBufferInternals));
  self->max = max;
  self->length = 0;
  self->buffer = calloc(max, sizeof(char));

  return self;
}

void CharBuffer_Add(CharBuffer self, char character) {
  if(character && self->length < self->max) {
    self->buffer[self->length] = character;
    self->length++;
  }
}

void CharBuffer_Erase(CharBuffer self) {
  if(self->length) {
    self->length--;
    self->buffer[self->length] = 0;
  }
}

char* CharBuffer_Value(CharBuffer self) {
  return self->buffer;
}

int CharBuffer_Max(CharBuffer self) {
  return self->max;
}

int CharBuffer_Length(CharBuffer self) {
  return self->length;
}

void CharBuffer_Clear(CharBuffer self) {
  for(int i=0; i < self->max; i++) {
    self->buffer[i]=0;
  }
  self->length = 0;
}

void CharBuffer_Destroy(CharBuffer self) {
  free(self->buffer);
  free(self);
}
