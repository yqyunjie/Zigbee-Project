//This file pulls in the appropriate register headers based on the
//specific Cortex-M3 being compiled.

#if defined (CORTEXM3_EM351)
  #include "em351/regs.h"
#elif defined (CORTEXM3_EM357)
  #include "em357/regs.h"
#elif defined (CORTEXM3_EM3581)
  #include "em3581/regs.h"
#elif defined (CORTEXM3_EM3582)
  #include "em3582/regs.h"
#elif defined (CORTEXM3_EM3585)
  #include "em3585/regs.h"
#elif defined (CORTEXM3_EM3586)
  #include "em3586/regs.h"
#elif defined (CORTEXM3_EM3588)
  #include "em3588/regs.h"
#elif defined (CORTEXM3_EM359)
  #include "em359/regs.h"
#endif

