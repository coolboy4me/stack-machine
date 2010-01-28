/*
 * Made in 2010 by Christian Stigen Larsen
 * http://csl.sublevel3.org
 *
 * Placed in the public domain by the author.
 *
 * This is a very simple stack machine.
 *
 * It's Turing complete, and you can do anything you want
 * with it, as long as you do it within 1 Mb of memory.
 *
 * Currently, the only program it accepts is hardcoded.
 * Program and memory size will be parameters later on.
 *
 * To stop your programs, just loop forever, as in embedded systems;
 * if you jump to the current line, this is understood as an
 * infinite loop and main() will exit.  The code for that is:
 *
 * load(PUSH); load(ip+sizeof(int32_t)); load(JMP);
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <stdarg.h>
#include <vector>
#include "sm-core.hpp"

const char* to_s(Op op)
{
  switch ( op ) {
  default:  return "<?>"; break;
  case NOP: return "NOP"; break;
  case ADD: return "ADD"; break;
  case SUB: return "SUB"; break;
  case AND: return "AND"; break;
  case OR:  return "OR"; break;
  case XOR: return "XOR"; break;
  case NOT: return "NOT"; break;
  case IN:  return "IN"; break;
  case OUT: return "OUT"; break;
  case LOAD: return "LOAD"; break;
  case STOR: return "STOR"; break;
  case JMP: return "JMP"; break;
  case JZ:  return "JZ"; break;
  case PUSH:  return "PUSH"; break;
  case DUP:  return "DUP"; break;
  case SWAP: return "SWAP"; break;
  case ROL3: return "ROL3"; break;
  }
}

fileptr::fileptr(FILE *file) : f(file)
{
  if ( f == NULL ) {
    fprintf(stderr, "Could not open file");
    exit(1);
  }
}

fileptr::~fileptr()
{
  fclose(f);
}

fileptr::operator FILE*() const
{
  return f;
}

machine_t::machine_t(const size_t memory_size,
  FILE* out,
  FILE* in) :
  memsize(memory_size),
  memory(new int32_t[memory_size]),
  fout(out),
  fin(in),
  running(true)
{
  reset();
}

void machine_t::reset()
{
  memset(memory, NOP, memsize*sizeof(int32_t));
  stack.clear();
  ip = 0;
}

machine_t::~machine_t()
{
  delete[](memory);
}

void machine_t::error(const char* s) const
{
  fprintf(stderr, "%s\n", s);
  // now what?
  // - push(ip), jump to specified address, pop on return
  // - stop executing
  // - throw
}

void machine_t::push(const int32_t& n)
{
  stack.push_back(n);
}

int32_t machine_t::pop()
{
  if ( stack.size() == 0 ) {
    error("POP empty stack");
    return 0;
  }

  int32_t n = stack.back();
  stack.pop_back();
  return n;
}

void machine_t::check_bounds(int32_t n, const char* msg)
{
  if ( n>=0 && n<memsize )
    return;

  error(msg);
}

void machine_t::next()
{
  ip += sizeof(int32_t);
  if ( ip >= memsize )
    ip = 0;
}

void machine_t::load(Op op)
{
  memory[ip] = op;
  next();
}

void machine_t::load(int32_t n)
{
  memory[ip] = n;
  next();
}

int machine_t::run(int32_t start_address)
{
  ip = start_address;

  while(running)
    eval(static_cast<Op>(memory[ip]));
}

void machine_t::eval(Op op)
{
  int32_t a=NOP, b=NOP, c=NOP;

  switch(op) {
  case NOP:
    next();
    break;

  case ADD:
    push(pop() + pop());
    next();
    break;

  case SUB:
    a = pop();
    b = pop();
    push(a-b);
    next();
    break;

  case AND:
    push(pop() & pop());
    next();
    break;

  case OR:
    push(pop() | pop());
    next();
    break;

  case XOR:
    push(pop() ^ pop());
    next();
    break;

  case NOT:
    push(!pop());
    next();
    break;

  case IN:
    push(getc(fin));
    next();
    break;

  case OUT:
    putc(pop(), fout);
    next();
    break;

  case LOAD:
    a = pop();
    check_bounds(a, "LOAD");
    push(memory[a]);
    next();
    break;

  case STOR:
    a = pop();
    b = pop();
    check_bounds(a, "STOR");
    memory[a] = b;
    next();
    break;

  case JMP:
    a = pop();
    check_bounds(a, "JMP");  

    // check if we are halting, i.e. jumping to current
    // address -- if so, quit
    if ( a == ip )
      running = false;
    else
      ip = a;

    break;

  case JZ:
    a = pop();

    if ( a != 0 )
      next();
    else {
      check_bounds(a, "JZ");
      ip = a; // jump
    }
    break;

  case PUSH:
    next();
    push(memory[ip]);
    next();
    break;

  case DUP:
    a = pop();
    push(a);
    push(a);
    next();
    break;

  case SWAP: // a, b -- b, a
    b = pop();
    a = pop();
    push(b);
    push(a);
    next();
    break;

  case ROL3: // abc -> bca
    c = pop();
    b = pop();
    a = pop();
    push(b);
    push(c);
    push(a);
    next();
    break;
  }
}

int32_t* machine_t::find_end() const
{
  // find end of program by scanning
  // backwards until non-NOP is found
  int32_t *p = &memory[memsize-1];
  while ( *p == NOP ) --p;
  return p;
}

void machine_t::load_image(FILE* f)
{
  reset();

  while ( !feof(f) ) {
    Op op = NOP;
    fread(&op, sizeof(Op), 1, f);
    load(op);
  }

  ip = 0;
}

void machine_t::save_image(FILE* f) const
{
  int32_t *start = memory;
  int32_t *end = find_end() + sizeof(int32_t);

  while ( start != end ) {
    int w = fwrite(start, sizeof(Op), 1, f);
    start += sizeof(int32_t);
  }
}