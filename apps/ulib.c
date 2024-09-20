
//
// wrapper so that it's OK if main() does not call exit().
//
void
_start()
{
  extern int main();
  main();
//  exit(0);
}