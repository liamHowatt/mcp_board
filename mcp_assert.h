#pragma once

#ifdef MCP_HANGING_ASSERT
    #define assert(expr) do { if(!(expr)) while(1); } while(0)
#else
    #include <assert.h>
#endif
