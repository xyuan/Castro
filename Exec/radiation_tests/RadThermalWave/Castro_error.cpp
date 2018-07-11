
#include "Castro.H"
#include "Castro_error_F.H"

using namespace amrex;

static Box the_same_box (const Box& b) { return b; }
static Box grow_box_by_one (const Box& b) { return amrex::grow(b,1); }
static Box grow_box_by_two (const Box& b) { return amrex::grow(b,2); }

typedef StateDescriptor::BndryFunc BndryFunc;

void
Castro::ErrorSetUp ()
{
    //
    // DEFINE ERROR ESTIMATION QUANTITIES
    //

  //  err_list.add("Temp",2,ErrorRec::Special,ca_laplac_error);
  err_list.add("Temp",1,ErrorRec::Special,ca_temperror);
}