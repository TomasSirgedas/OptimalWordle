// on Windows, this CPP makes `std::cout` output to the "Output" panel in addition to the usual output console
// (note: the output is buffered until std::endl is streamed)

#ifdef _WIN32

#include <iostream>
#include <string>
#include <Windows.h>

namespace std
{
   class trace_streambuf : public basic_streambuf<char, char_traits<char>>
   {
   public:
      trace_streambuf() : originalCoutStream( cout.rdbuf() )
      {
         cout.rdbuf( this );         
      }
      int_type overflow( int_type x )
      {
         buffer += (char)x;
         if ( x == 10 )
         {
            originalCoutStream << buffer;
            ::OutputDebugStringA( buffer.c_str() );
            buffer.clear();
         }
         return 0;
      }
   private:
      basic_ostream<char, std::char_traits<char>> originalCoutStream;
      string buffer;
   };
   trace_streambuf trace_buf;
   basic_ostream<char, std::char_traits<char>> trace( &trace_buf );
}

#endif
