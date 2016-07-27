#include <winstd.H>

#ifndef WIN32
#include <unistd.h>
#endif

#include <iomanip>

#include <algorithm>
#include <cstdio>
#include <vector>
#include <iostream>
#include <string>
#include <ctime>

using std::cout;
using std::cerr;
using std::endl;
using std::istream;
using std::ostream;
using std::pair;
using std::string;

#include <Utility.H>
#include <CONSTANTS.H>
#include <Castro.H>
#include <Castro_F.H>
#include <Derive_F.H>
#include <VisMF.H>
#include <TagBox.H>
#include <ParmParse.H>
#include <Castro_error_F.H>

#ifdef RADIATION
#include "Radiation.H"
#endif

#ifdef PARTICLES
#include <Particles_F.H>
#endif

#ifdef GRAVITY
#include "Gravity.H"
#endif

#ifdef DIFFUSION
#include "Diffusion.H"
#endif

#ifdef LEVELSET
#include "LevelSet_F.H"
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

bool         Castro::signalStopJob = false;

int          Castro::checkpoint_version = 1;

bool         Castro::dump_old      = false;

int          Castro::verbose       = 0;
ErrorList    Castro::err_list;
int          Castro::radius_grow   = 1;
BCRec        Castro::phys_bc;
int          Castro::NUM_STATE     = -1;
int          Castro::do_reflux     = 1;
int          Castro::NUM_GROW      = -1;

Real         Castro::frac_change   = 1.e200;

int          Castro::Density       = -1;
int          Castro::Eden          = -1;
int          Castro::Eint          = -1;
int          Castro::Temp          = -1;
int          Castro::Xmom          = -1;
int          Castro::Ymom          = -1;
int          Castro::Zmom          = -1;
#ifdef HYBRID_MOMENTUM
int          Castro::Rmom          = -1;
int          Castro::Lmom          = -1;
int          Castro::Pmom          = -1;
#endif

int          Castro::NumSpec       = 0;
int          Castro::FirstSpec     = -1;

int          Castro::NumAux        = 0;
int          Castro::FirstAux      = -1;

int          Castro::NumAdv        = 0;
int          Castro::FirstAdv      = -1;

#ifdef SHOCK_VAR
int          Castro::Shock         = -1;
#endif

int          Castro::QVAR          = -1;

#include <castro_defaults.H>

#ifdef GRAVITY
// the gravity object
Gravity*     Castro::gravity  = 0;
#endif

#ifdef DIFFUSION
// the diffusion object
Diffusion*    Castro::diffusion  = 0;

int           Castro::diffuse_temp = 0;
int           Castro::diffuse_enth = 0;
int           Castro::diffuse_spec = 0;
int           Castro::diffuse_vel = 0;
Real          Castro::diffuse_cutoff_density = -1.e200;
#endif

#ifdef RADIATION
int          Castro::do_radiation = -1;

// the radiation object
Radiation*   Castro::radiation = 0;
#endif


std::string  Castro::probin_file = "probin";                                    


#if BL_SPACEDIM == 1
IntVect      Castro::hydro_tile_size(1024);
#elif BL_SPACEDIM == 2
IntVect      Castro::hydro_tile_size(1024,16);
#else
IntVect      Castro::hydro_tile_size(1024,16,16);
#endif

// this will be reset upon restart
Real         Castro::previousCPUTimeUsed = 0.0;

Real         Castro::startCPUTime = 0.0;

// Note: Castro::variableSetUp is in Castro_setup.cpp

void
Castro::variableCleanUp () 
{
#ifdef GRAVITY
  if (gravity != 0) {
    if (verbose > 1 && ParallelDescriptor::IOProcessor()) {
      cout << "Deleting gravity in variableCleanUp..." << '\n';
    }
    delete gravity;
    gravity = 0;
  }
#endif

#ifdef DIFFUSION
  if (diffusion != 0) {
    if (verbose > 1 && ParallelDescriptor::IOProcessor()) {
      cout << "Deleting diffusion in variableCleanUp..." << '\n';
    }
    delete diffusion;
    diffusion = 0;
  }
#endif

#ifdef RADIATION
  if (radiation != 0) { int report = (verbose || radiation->verbose);
    if (report && ParallelDescriptor::IOProcessor()) {
      cout << "Deleting radiation in variableCleanUp..." << '\n';
    }
    delete radiation;
    radiation = 0;
    if (report && ParallelDescriptor::IOProcessor()) {
      cout << "                                        done" << endl;
    }
  }
#endif

#ifdef PARTICLES
  delete TracerPC;
  TracerPC = 0;
#endif

    desc_lst.clear();
}

void
Castro::read_params ()
{
    static bool done = false;

    if (done) return;

    done = true;

    ParmParse pp("castro");   

#include <castro_queries.H>

    pp.query("v",verbose);
    pp.query("sum_interval",sum_interval);
    pp.query("do_reflux",do_reflux);
    do_reflux = (do_reflux ? 1 : 0);

    pp.query("dump_old",dump_old);

    // Get boundary conditions
    Array<int> lo_bc(BL_SPACEDIM), hi_bc(BL_SPACEDIM);
    pp.getarr("lo_bc",lo_bc,0,BL_SPACEDIM);
    pp.getarr("hi_bc",hi_bc,0,BL_SPACEDIM);
    for (int i = 0; i < BL_SPACEDIM; i++)
    {
        phys_bc.setLo(i,lo_bc[i]);
        phys_bc.setHi(i,hi_bc[i]);
    }

    //
    // Check phys_bc against possible periodic geometry
    // if periodic, must have internal BC marked.
    //
    if (Geometry::isAnyPeriodic())
    {
        //
        // Do idiot check.  Periodic means interior in those directions.
        //
        for (int dir = 0; dir<BL_SPACEDIM; dir++)
        {
            if (Geometry::isPeriodic(dir))
            {
                if (lo_bc[dir] != Interior)
                {
                    std::cerr << "Castro::read_params:periodic in direction "
                              << dir
                              << " but low BC is not Interior\n";
                    BoxLib::Error();
                }
                if (hi_bc[dir] != Interior)
                {
                    std::cerr << "Castro::read_params:periodic in direction "
                              << dir
                              << " but high BC is not Interior\n";
                    BoxLib::Error();
                }
            }
        }
    }
    else
    {
        //
        // Do idiot check.  If not periodic, should be no interior.
        //
        for (int dir=0; dir<BL_SPACEDIM; dir++)
        {
            if (lo_bc[dir] == Interior)
            {
                std::cerr << "Castro::read_params:interior bc in direction "
                          << dir
                          << " but not periodic\n";
                BoxLib::Error();
            }
            if (hi_bc[dir] == Interior)
            {
                std::cerr << "Castro::read_params:interior bc in direction "
                          << dir
                          << " but not periodic\n";
                BoxLib::Error();
            }
        }
    }

    if ( Geometry::IsRZ() && (lo_bc[0] != Symmetry) ) {
        std::cerr << "ERROR:Castro::read_params: must set r=0 boundary condition to Symmetry for r-z\n";
        BoxLib::Error();
    }

#if (BL_SPACEDIM == 1)
    if ( Geometry::IsSPHERICAL() )
    {
      if ( (lo_bc[0] != Symmetry) && (Geometry::ProbLo(0) == 0.0) ) 
      {
        std::cerr << "ERROR:Castro::read_params: must set r=0 boundary condition to Symmetry for spherical\n";
        BoxLib::Error();
      }
    }
#elif (BL_SPACEDIM == 2)
    if ( Geometry::IsSPHERICAL() )
      {
	BoxLib::Abort("We don't support spherical coordinate systems in 2D");
      }
#elif (BL_SPACEDIM == 3)
    if ( Geometry::IsRZ() )
      {
	BoxLib::Abort("We don't support cylindrical coordinate systems in 3D"); 
      }
    else if ( Geometry::IsSPHERICAL() )
      {
	BoxLib::Abort("We don't support spherical coordinate systems in 3D");
      }
#endif


#ifdef DIFFUSION
    pp.query("diffuse_temp",diffuse_temp);
    pp.query("diffuse_enth",diffuse_enth);
    pp.query("diffuse_spec",diffuse_spec);
    pp.query("diffuse_vel",diffuse_vel);
    pp.query("diffuse_cutoff_density",diffuse_cutoff_density);
#endif

    // sanity checks

    if (grown_factor < 1) 
       BoxLib::Error("grown_factor must be integer >= 1");

    if (ppm_reference > 1 || ppm_reference < 0)
      {
        std::cerr << "invalid ppm_reference\n";
        BoxLib::Error();
      }	

    if (cfl <= 0.0 || cfl > 1.0)
      BoxLib::Error("Invalid CFL factor; must be between zero and one.");

    // for the moment, ppm_type = 0 does not support ppm_trace_sources --
    // we need to add the momentum sources to the states (and not
    // add it in trans_3d
    if (ppm_type == 0 && ppm_trace_sources == 1)
      {
        std::cerr << "ppm_trace_sources = 1 not implemented for ppm_type = 0 \n";
        BoxLib::Error();
      }

#ifdef RADIATION
    if (ppm_trace_sources == 1)
      {
        std::cerr << "ppm_trace_sources = 1 not implemented for radiation yet \n";
	BoxLib::Error();
      }
#endif


    if (ppm_temp_fix > 0 && BL_SPACEDIM == 1)
      {
        std::cerr << "ppm_temp_fix > 0 not implemented in 1-d \n";
        BoxLib::Error();
      }

    if (ppm_tau_in_tracing == 1 && BL_SPACEDIM == 1)
      {
        std::cerr << "ppm_tau_in_tracing == 1 not implemented in 1-d \n";
        BoxLib::Error();
      }

    if (ppm_predict_gammae == 1 && ppm_tau_in_tracing != 1)
      {
	std::cerr << "ppm_predict_gammae == 1 needs ppm_tau_in_tracing == 1\n";
	BoxLib::Error();
      }

    if (hybrid_riemann == 1 && BL_SPACEDIM == 1)
      {
        std::cerr << "hybrid_riemann only implemented in 2- and 3-d\n";
        BoxLib::Error();
      }

    if (hybrid_riemann == 1 && (Geometry::IsSPHERICAL() || Geometry::IsRZ() ))
      {
        std::cerr << "hybrid_riemann should only be used for Cartesian coordinates\n";
        BoxLib::Error();
      }

    if (use_colglaz >= 0)
      {
	std::cerr << "ERROR:: use_colglaz is deprecated.  Use riemann_solver instead\n";
	BoxLib::Error();
      }


    // Make sure not to call refluxing if we're not actually doing any hydro.
    if (do_hydro == 0) do_reflux = 0;

    if (max_dt < fixed_dt)
      {
	std::cerr << "cannot have max_dt < fixed_dt\n";
	BoxLib::Error();
      }

#ifdef PARTICLES
    read_particle_params();
#endif

#ifdef RADIATION
    pp.get("do_radiation",do_radiation);

    // Some radiation parameters are initialized here because they
    // may be used in variableSetUp, well before the call to the
    // Radiation constructor, 

    if (do_radiation) {
      Radiation::read_static_params();
    }
#endif

#ifdef ROTATION
    if (do_rotation) {
      if (rotational_period <= 0.0) {
	std::cerr << "Error:Castro::Rotation enabled but rotation period less than zero\n";
	BoxLib::Error();
      }
    }
    if (Geometry::IsRZ())
      rot_axis = 2;
#if (BL_SPACEDIM == 1)
      if (do_rotation) {
	std::cerr << "ERROR:Castro::Rotation not implemented in 1d\n";
	BoxLib::Error();
      }
#endif
#endif

   StateDescriptor::setBndryFuncThreadSafety(bndry_func_thread_safe);

   ParmParse ppa("amr");
   ppa.query("probin_file",probin_file);

    Array<int> tilesize(BL_SPACEDIM);
    if (pp.queryarr("hydro_tile_size", tilesize, 0, BL_SPACEDIM))
    {
	for (int i=0; i<BL_SPACEDIM; i++) hydro_tile_size[i] = tilesize[i];
    }
}

Castro::Castro ()
{
    flux_reg = 0;
#ifdef RADIATION
    rad_flux_reg = 0;
#endif
    fine_mask = 0;
}

Castro::Castro (Amr&            papa,
                int             lev,
                const Geometry& level_geom,
                const BoxArray& bl,
                Real            time)
    :
    AmrLevel(papa,lev,level_geom,bl,time),
    comp_minus_level_grad_phi(BL_SPACEDIM),
#ifdef RADIATION
    rad_fluxes(BL_SPACEDIM),
#endif
    fluxes(3),
    u_gdnv(BL_SPACEDIM),
    old_sources(num_src, PArrayManage),
    new_sources(num_src, PArrayManage),
    prev_state(NUM_STATE_TYPE, PArrayManage)
{
    buildMetrics();

    for (int i = 0; i < n_lost; i++) {
      material_lost_through_boundary_cumulative[i] = 0.0;
      material_lost_through_boundary_temp[i] = 0.;
    }

    flux_reg = 0;
    if (level > 0 && do_reflux)
    {
        flux_reg = new FluxRegister(grids,crse_ratio,level,NUM_STATE);
        flux_reg->setVal(0.0);
    }

#ifdef RADIATION    
    rad_flux_reg = 0;
    if (Radiation::rad_hydro_combined && level > 0 && do_reflux) 
    {
      rad_flux_reg = new FluxRegister(grids,crse_ratio,level,Radiation::nGroups);
      rad_flux_reg->setVal(0.0);
    }
#endif

    fine_mask = 0;

#ifdef GRAVITY

   // Initialize to zero here in case we run with do_grav = false.
   MultiFab& new_grav_mf = get_new_data(Gravity_Type);
   new_grav_mf.setVal(0.0);
       
   if (do_grav) {
      // gravity is a static object, only alloc if not already there
      if (gravity == 0) 
	gravity = new Gravity(parent,parent->finestLevel(),&phys_bc,Density);

#if (BL_SPACEDIM > 1)
      // Passing numpts_1d at level 0 
      if (!Geometry::isAllPeriodic() && gravity != 0)
      {
         int numpts_1d = get_numpts();
         gravity->set_numpts_in_gravity(numpts_1d);
      }
#endif

      gravity->install_level(level,this,volume,area);

      if (verbose && level == 0 &&  ParallelDescriptor::IOProcessor()) 
         std::cout << "Setting the gravity type to " << gravity->get_gravity_type() << std::endl;

       // We need to initialize this to zero since certain bc types don't overwrite the potential NaNs 
       // ghost cells because they are only multiplying them by a zero coefficient.
       MultiFab& phi_new = get_new_data(PhiGrav_Type);
       phi_new.setVal(0.0,phi_new.nGrow());

   } else {
       MultiFab& phi_new = get_new_data(PhiGrav_Type);
       phi_new.setVal(0.0);
   }

#endif

#ifdef ROTATION

   // Initialize rotation data to zero.

   MultiFab& phirot_new = get_new_data(PhiRot_Type);
   phirot_new.setVal(0.0);

   MultiFab& rot_new = get_new_data(Rotation_Type);
   rot_new.setVal(0.0);

#endif

   // Initialize source term data to zero.

   MultiFab& dSdt_new = get_new_data(Source_Type);
   dSdt_new.setVal(0.0);
   
#ifdef REACTIONS

   // Initialize reaction data to zero.

   MultiFab& reactions_new = get_new_data(Reactions_Type);
   reactions_new.setVal(0.0);

   // Initialize reactions source term to zero.

   react_src = new MultiFab(grids, QVAR, NUM_GROW, Fab_allocate);
   react_src->setVal(0.0);

#ifdef SDC
     for (int n = 0; n < num_src; ++n) {
       old_sources.set(n, new MultiFab(grids, NUM_STATE, NUM_GROW, Fab_allocate));
       new_sources.set(n, new MultiFab(grids, NUM_STATE, NUM_GROW, Fab_allocate));

       old_sources[n].setVal(0.0, NUM_GROW);
       new_sources[n].setVal(0.0, NUM_GROW);
     }
#endif

#endif
   
#ifdef DIFFUSION
      // diffusion is a static object, only alloc if not already there
      if (diffusion == 0) 
	diffusion = new Diffusion(parent,&phys_bc);

      diffusion->install_level(level,this,volume,area);
#endif

#ifdef RADIATION
    if (do_radiation) {
      if (radiation == 0) {
	// radiation is a static object, only alloc if not already there
	radiation = new Radiation(parent, this);
      }
      radiation->regrid(level, grids);
    }
#endif


    // initialize the Godunov state array used in hydro -- we wait
    // until here so that ngroups is defined (if needed) in
    // rad_params_module
#ifdef RADIATION
    init_godunov_indices_rad();
#else
    init_godunov_indices();
#endif

#ifdef LEVELSET
    // Build level set narrowband helpers
    LStype.define(bl,1,1,Fab_allocate);
    LSnband.define(bl,BL_SPACEDIM,1,Fab_allocate);
    LSmine.define(bl,BL_SPACEDIM,1,Fab_allocate);
#endif

}

Castro::~Castro () 
{
    delete flux_reg;

#ifdef RADIATION
    if (Radiation::rad_hydro_combined) {
      delete rad_flux_reg;
    }
    if (radiation != 0) {
      //radiation->cleanup(level);
      radiation->close(level);
    }
#endif
    delete fine_mask;

#ifdef REACTIONS
    delete react_src;
#endif
}


void
Castro::buildMetrics ()
{
    const int ngrd = grids.size();

    radius.resize(ngrd);

    const Real* dx = geom.CellSize();

    for (int i = 0; i < ngrd; i++)
    {
        const Box& b = grids[i];
        int ilo      = b.smallEnd(0)-radius_grow;
        int ihi      = b.bigEnd(0)+radius_grow;
        int len      = ihi - ilo + 1;

        radius[i].resize(len);

        Real* rad = radius[i].dataPtr();

        if (Geometry::IsCartesian())
        {
            for (int j = 0; j < len; j++)
            {
                rad[j] = 1.0;
            }
        }
        else
        {
            RealBox gridloc = RealBox(grids[i],geom.CellSize(),geom.ProbLo());

            const Real xlo = gridloc.lo(0) + (0.5 - radius_grow)*dx[0];

            for (int j = 0; j < len; j++)
            {
                rad[j] = xlo + j*dx[0];
            }
        }
    }

    volume.clear();
    volume.define(grids,1,NUM_GROW,Fab_allocate);
    geom.GetVolume(volume);

    for (int dir = 0; dir < BL_SPACEDIM; dir++)
    {
        area[dir].clear();
	area[dir].define(getEdgeBoxArray(dir),1,NUM_GROW,Fab_allocate);
        geom.GetFaceArea(area[dir],dir);
    }

    dLogArea[0].clear();
#if (BL_SPACEDIM <= 2)
    geom.GetDLogA(dLogArea[0],grids,0,NUM_GROW);
#endif

    if (level == 0) setGridInfo();    
}

void
Castro::setTimeLevel (Real time,
                      Real dt_old,
                      Real dt_new)
{
    AmrLevel::setTimeLevel(time,dt_old,dt_new);
}

void
Castro::setGridInfo ()
{

    // Send refinement data to Fortran. We do it here
    // because now the grids have been initialized and
    // we need this data for setting up the problem.
    // Note that this routine will always get called
    // on level 0, even if we are doing a restart,
    // so it is safe to put this here. 

    if (level == 0) {

      int max_level = parent->maxLevel();
      int nlevs = max_level + 1;
    
      Real dx_level[3*nlevs];
      int domlo_level[3*nlevs];
      int domhi_level[3*nlevs];
      int ref_ratio_to_f[3*nlevs];
      int n_error_buf_to_f[nlevs];
      int blocking_factor_to_f[nlevs];

      const Real* dx_coarse = geom.CellSize();

      const int* domlo_coarse = geom.Domain().loVect();
      const int* domhi_coarse = geom.Domain().hiVect();

      for (int dir = 0; dir < 3; dir++) {
	dx_level[dir] = (ZFILL(dx_coarse))[dir];

	domlo_level[dir] = (ARLIM_3D(domlo_coarse))[dir];
	domhi_level[dir] = (ARLIM_3D(domhi_coarse))[dir];

	// Refinement ratio and error buffer on finest level are meaningless,
	// and we want them to be zero on the finest level because some
	// of the algorithms depend on this feature.

	ref_ratio_to_f[dir + 3 * (nlevs - 1)] = 0;
	n_error_buf_to_f[nlevs-1] = 0;
      }

      for (int lev = 0; lev <= max_level; lev++)
	blocking_factor_to_f[lev] = parent->blockingFactor(lev);

      for (int lev = 1; lev <= max_level; lev++) {
	IntVect ref_ratio = parent->refRatio(lev-1);

	// Note that we are explicitly calculating here what the
	// data would be on refined levels rather than getting the
	// data directly from those levels, because some potential
	// refined levels may not exist at the beginning of the simulation.
      
	for (int dir = 0; dir < 3; dir++)
	  if (dir < BL_SPACEDIM) {
	    dx_level[3 * lev + dir] = dx_level[3 * (lev - 1) + dir] / ref_ratio[dir];
	    int ncell = (domhi_level[3 * (lev - 1) + dir] - domlo_level[3 * (lev - 1) + dir] + 1) * ref_ratio[dir];
	    domlo_level[3 * lev + dir] = domlo_level[dir];
	    domhi_level[3 * lev + dir] = domlo_level[3 * lev + dir] + ncell - 1;
	    ref_ratio_to_f[3 * (lev - 1) + dir] = ref_ratio[dir];
	  } else {
	    dx_level[3 * lev + dir] = 0.0;
	    domlo_level[3 * lev + dir] = 0;
	    domhi_level[3 * lev + dir] = 0;
	    ref_ratio_to_f[3 * (lev - 1) + dir] = 0;
	  }

	n_error_buf_to_f[lev - 1] = parent->nErrorBuf(lev - 1);
      }

      set_grid_info(max_level, dx_level, domlo_level, domhi_level,
		    ref_ratio_to_f, n_error_buf_to_f, blocking_factor_to_f);

    }
    
}

void
Castro::initData ()
{
    BL_PROFILE("Castro::initData()");

    //
    // Loop over grids, call FORTRAN function to init with data.
    //
    int ns          = NUM_STATE;
    const Real* dx  = geom.CellSize();
    MultiFab& S_new = get_new_data(State_Type);
    Real cur_time   = state[State_Type].curTime();
    
    S_new.setVal(0.);

    // make sure dx = dy = dz -- that's all we guarantee to support
#if (BL_SPACEDIM == 2)
    const Real SMALL = 1.e-13;
    if (fabs(dx[0] - dx[1]) > SMALL*dx[0])
      {
	BoxLib::Abort("We don't support dx != dy");
      }
#elif (BL_SPACEDIM == 3)
    const Real SMALL = 1.e-13;
    if ( (fabs(dx[0] - dx[1]) > SMALL*dx[0]) || (fabs(dx[0] - dx[2]) > SMALL*dx[0]) )
      {
	BoxLib::Abort("We don't support dx != dy != dz");
      }
#endif

    set_amr_info(level, -1, -1, -1.0, -1.0);

    if (verbose && ParallelDescriptor::IOProcessor())
       std::cout << "Initializing the data at level " << level << std::endl;

#ifdef RADIATION
    // rad quantities are in the state even if (do_radiation == 0)
    MultiFab &Rad_new = get_new_data(Rad_Type);
    Rad_new.setVal(0.);
#endif

#ifdef REACTIONS
    MultiFab &React_new = get_new_data(Reactions_Type);
    React_new.setVal(0.);
#endif

#ifdef MAESTRO_INIT
    MAESTRO_init();
#else
    {
       for (MFIter mfi(S_new); mfi.isValid(); ++mfi)
       {
	  RealBox gridloc = RealBox(grids[mfi.index()],geom.CellSize(),geom.ProbLo());	 
          const Box& box     = mfi.validbox();
          const int* lo      = box.loVect();
          const int* hi      = box.hiVect();
  
#ifdef DIMENSION_AGNOSTIC
          BL_FORT_PROC_CALL(CA_INITDATA,ca_initdata)
          (level, cur_time, ARLIM_3D(lo), ARLIM_3D(hi), ns,
  	   BL_TO_FORTRAN_3D(S_new[mfi]), ZFILL(dx),
  	   ZFILL(gridloc.lo()), ZFILL(gridloc.hi()));
#else
          BL_FORT_PROC_CALL(CA_INITDATA,ca_initdata)
  	  (level, cur_time, lo, hi, ns,
  	   BL_TO_FORTRAN(S_new[mfi]), dx,
  	   gridloc.lo(), gridloc.hi());
#endif

	  // Generate the initial hybrid momenta based on this user data.

#ifdef HYBRID_MOMENTUM
	  init_hybrid_momentum(lo, hi, BL_TO_FORTRAN_3D(S_new[mfi]));
#endif

          // Verify that the sum of (rho X)_i = rho at every cell
          ca_check_initial_species(ARLIM_3D(lo), ARLIM_3D(hi), BL_TO_FORTRAN_3D(S_new[mfi]));
       }
       enforce_consistent_e(S_new);
    }

#ifdef RADIATION
    if (do_radiation) {
      for (MFIter mfi(S_new); mfi.isValid(); ++mfi) {
          int i = mfi.index();

	  if (radiation->verbose > 2) {
	    cout << "Calling RADINIT at level " << level << ", grid "
		 << i << endl;
	  }

          RealBox    gridloc(grids[mfi.index()],
                             geom.CellSize(), geom.ProbLo());
          const Box& box = mfi.validbox();
          const int* lo  = box.loVect();
          const int* hi  = box.hiVect();

	  Rad_new[mfi].setVal(0.0);

	  BL_FORT_PROC_CALL(CA_INITRAD,ca_initrad)
	      (level, cur_time, lo, hi, Radiation::nGroups,
	       BL_TO_FORTRAN(Rad_new[mfi]),dx,
	       gridloc.lo(),gridloc.hi());

	  if (Radiation::nNeutrinoSpecies > 0 && Radiation::nNeutrinoGroups[0] == 0) {
	      // Hack: running photon radiation through neutrino solver
            Rad_new[mfi].mult(Radiation::Etorad, 
                            0, Radiation::nGroups);
	  }

          if (Rad_new.nComp() > Radiation::nGroups) {
            // Initialize flux components to 0
            Rad_new[mfi].setVal(0.0, box, Radiation::nGroups,
                              Rad_new.nComp() - Radiation::nGroups);
          }
      }
    }
#endif // RADIATION

#endif // MAESTRO_INIT

    set_special_tagging_flag(cur_time);

#if (BL_SPACEDIM > 1)
    if ( (level == 0) && (spherical_star == 1) ) {
       int nc = S_new.nComp();
       int n1d = get_numpts();
       allocate_outflow_data(&n1d,&nc);
       int is_new = 1;
       make_radial_data(is_new);
    }
#endif

#ifdef GRAVITY
    MultiFab& G_new = get_new_data(Gravity_Type);
    G_new.setVal(0.);

    MultiFab& phi_new = get_new_data(PhiGrav_Type);
    phi_new.setVal(0.);
#endif

    MultiFab& dSdt_new = get_new_data(Source_Type);
    dSdt_new.setVal(0.);

#ifdef ROTATION
    MultiFab& rot_new = get_new_data(Rotation_Type);
    rot_new.setVal(0.);
    
    MultiFab& phirot_new = get_new_data(PhiRot_Type);
    phirot_new.setVal(0.);
#endif

#ifdef LEVELSET
    MultiFab& LS_new = get_new_data(LS_State_Type);
    LS_new.setVal(0.);

    for (MFIter mfi(LS_new); mfi.isValid(); ++mfi) 
      {        
        RealBox    gridloc = RealBox(grids[mfi.index()],geom.CellSize(),geom.ProbLo());
	IntFab& type       = LStype[mfi];
	const Box& box     = mfi.validbox();
        const int* lo      = box.loVect();
        const int* hi      = box.hiVect();

        BL_FORT_PROC_CALL(CA_INITPHI,ca_initphi)
	  (level, cur_time, lo, hi, 1,
	   BL_TO_FORTRAN(LS_new[mfi]),
	   BL_TO_FORTRAN(type),
	   dx,gridloc.lo(),gridloc.hi());
      }
#endif

#ifdef PARTICLES
    if (level == 0)
	init_particles();
#endif

    if (verbose && ParallelDescriptor::IOProcessor())
       std::cout << "Done initializing the level " << level << " data " << std::endl;
}

void
Castro::init (AmrLevel &old)
{
    BL_PROFILE("Castro::init(old)");

    Castro* oldlev = (Castro*) &old;

    //
    // Create new grid data by fillpatching from old.
    //
    Real dt_new    = parent->dtLevel(level);
    Real cur_time  = oldlev->state[State_Type].curTime();
    Real prev_time = oldlev->state[State_Type].prevTime();
    Real dt_old    = cur_time - prev_time;
    setTimeLevel(cur_time,dt_old,dt_new);

    MultiFab& S_new = get_new_data(State_Type);
    FillPatch(old,S_new,0,cur_time,State_Type,0,NUM_STATE);

    // Set E in terms of e + kinetic energy
    // enforce_consistent_e(S_new);

#ifdef RADIATION
    if (do_radiation) {
      MultiFab& Er_new = get_new_data(Rad_Type);
      int ncomp = Er_new.nComp();

      FillPatch(old,Er_new,0,cur_time,Rad_Type,0,ncomp);
    }
#endif

#ifdef GRAVITY
    if (do_grav) {
	MultiFab& phi_new = get_new_data(PhiGrav_Type);
	FillPatch(old,phi_new,0,cur_time,PhiGrav_Type,0,1);
    }
#endif

#ifdef ROTATION
    if (do_rotation) {
	MultiFab& phirot_new = get_new_data(PhiRot_Type);
	FillPatch(old,phirot_new,0,cur_time,PhiRot_Type,0,1);
    }
#endif

    MultiFab& dSdt_new = get_new_data(Source_Type);
    FillPatch(old,dSdt_new,0,cur_time,Source_Type,0,NUM_STATE);
    
#ifdef REACTIONS
    {
	MultiFab& React_new = get_new_data(Reactions_Type);
	int ncomp = React_new.nComp();

        FillPatch(old,React_new,0,cur_time,Reactions_Type,0,ncomp);
    }

#endif

#ifdef LEVELSET
    MultiFab& LS_new = get_new_data(LS_State_Type);
    int nGrowRegrid = 0;
    
    FillPatch(old,LS_new,nGrowRegrid,cur_time,LS_State_Type,0,1);
    
    // FIXME: Assumes that interpolated coarse data should rather just be setvald
    LStype.setVal(3); // This means we don't care about these points
    LStype.copy(oldlev->LStype);
    
    // Reinitialize to allow narrowband to push into new cell area
    // ...need to build narrowband structure prior to using it though
    for (MFIter mfi(LStype); mfi.isValid(); ++mfi)
      {
	IntFab& type = LStype[mfi];
	IntFab& nband = LSnband[mfi];
	IntFab& mine = LSmine[mfi];
	const Box& box = mfi.validbox();
	int nbandsize = nband.box().numPts();
	int minesize = mine.box().numPts();
	
	// Set nband data based on type
	BL_FORT_PROC_CALL(LS_NARROWBAND,ls_narrowband)
	     (BL_TO_FORTRAN(type),
	      nband.dataPtr(), &nbandsize,
	      mine.dataPtr(), &minesize,
	      box.loVect(), box.hiVect());
      }
    reinit_phi(cur_time);
#endif
}

//
// This version inits the data on a new level that did not
// exist before regridding.
//
void
Castro::init ()
{
    BL_PROFILE("Castro::init()");

    Real dt        = parent->dtLevel(level);
    Real cur_time  = getLevel(level-1).state[State_Type].curTime();
    Real prev_time = getLevel(level-1).state[State_Type].prevTime();

    Real dt_old = (cur_time - prev_time)/(Real)parent->MaxRefRatio(level-1);

    setTimeLevel(cur_time,dt_old,dt);
    MultiFab& S_new = get_new_data(State_Type);
    FillCoarsePatch(S_new, 0, cur_time, State_Type, 0, NUM_STATE);

    // Set E in terms of e + kinetic energy
    // enforce_consistent_e(S_new);

#ifdef RADIATION
    if (do_radiation) {
      MultiFab& Er_new = get_new_data(Rad_Type);
      int ncomp = Er_new.nComp();
      FillCoarsePatch(Er_new, 0, cur_time, Rad_Type, 0, ncomp);
    }
#endif

#ifdef GRAVITY
    if (do_grav) {
	MultiFab& phi_new = get_new_data(PhiGrav_Type);
	FillCoarsePatch(phi_new, 0, cur_time, PhiGrav_Type, 0, 1);
    }
#endif

    MultiFab& dSdt_new = get_new_data(Source_Type);
    FillCoarsePatch(dSdt_new, 0, cur_time, Source_Type, 0, NUM_STATE);
    
#ifdef ROTATION
    if (do_rotation) {
      MultiFab& phirot_new = get_new_data(PhiRot_Type);
      FillCoarsePatch(phirot_new, 0, cur_time, PhiRot_Type, 0, 1);
    }
#endif

#ifdef LEVELSET
    FillCoarsePatch(get_new_data(LS_State_Type),0,cur_time,LS_State_Type,0,1);
#endif

}

Real
Castro::initialTimeStep ()
{
    Real dummy_dt = 0.0;
    Real init_dt  = 0.0;

    if (initial_dt > 0.0) 
    {
       init_dt = initial_dt;
    } 
    else 
    {
       init_dt = init_shrink*estTimeStep(dummy_dt);
    }

    return init_dt;
}

Real
Castro::estTimeStep (Real dt_old)
{
    BL_PROFILE("Castro::estTimeStep()");

    if (fixed_dt > 0.0)
        return fixed_dt;

    set_amr_info(level, -1, -1, -1.0, -1.0);

    Real estdt = max_dt;

    const MultiFab& stateMF = get_new_data(State_Type);

    const Real* dx = geom.CellSize();

    std::string limiter = "castro.max_dt";

    // Start the hydro with the max_dt value, but divide by CFL
    // to account for the fact that we multiply by it at the end.
    // This ensures that if max_dt is more restrictive than the hydro
    // criterion, we will get exactly max_dt for a timestep.

    Real estdt_hydro = max_dt / cfl;

#ifdef DIFFUSION
    if (do_hydro or diffuse_temp or diffuse_enth)
#else
    if (do_hydro)
#endif
    {

#ifdef RADIATION
      if (Radiation::rad_hydro_combined) {

	  // Compute radiation + hydro limited timestep.
	
#ifdef _OPENMP
#pragma omp parallel
#endif
	  {
	      Real dt = max_dt / cfl;

	      const MultiFab& radMF = get_new_data(Rad_Type);
	      FArrayBox gPr;

	      for (MFIter mfi(stateMF, true); mfi.isValid(); ++mfi)
	      {
	          const Box& tbox = mfi.tilebox();
	          const Box& vbox = mfi.validbox();

		  gPr.resize(tbox);
		  radiation->estimate_gamrPr(stateMF[mfi], radMF[mfi], gPr, dx, vbox);

		  ca_estdt_rad(BL_TO_FORTRAN(stateMF[mfi]),
			       BL_TO_FORTRAN(gPr),
			       tbox.loVect(),tbox.hiVect(),dx,&dt);
              }
#ifdef _OPENMP
#pragma omp critical (castro_estdt_rad)
#endif
	      {
	          estdt_hydro = std::min(estdt_hydro,dt);
              }
          }
      }
      else
      {
#endif

	  // Compute hydro-limited timestep.
	if (do_hydro)
	  {

#ifdef _OPENMP
#pragma omp parallel
#endif
	    {
	      Real dt = max_dt / cfl;

	      for (MFIter mfi(stateMF,true); mfi.isValid(); ++mfi)
		{
		  const Box& box = mfi.tilebox();

		  ca_estdt(ARLIM_3D(box.loVect()), ARLIM_3D(box.hiVect()),
			   BL_TO_FORTRAN_3D(stateMF[mfi]),
			   ZFILL(dx),&dt);
		}
#ifdef _OPENMP
#pragma omp critical (castro_estdt)
#endif
	      {
		estdt_hydro = std::min(estdt_hydro,dt);
	      }
	    }
	  }

#ifdef DIFFUSION
	// Diffusion-limited timestep
	// Note that the diffusion uses the same CFL safety factor
	// as the main hydrodynamics timestep limiter.
	if (diffuse_temp)
	{
#ifdef _OPENMP
#pragma omp parallel
#endif
	    {
	      Real dt = max_dt / cfl;

	      for (MFIter mfi(stateMF,true); mfi.isValid(); ++mfi)
		{
		  const Box& box = mfi.tilebox();
		  ca_estdt_temp_diffusion(ARLIM_3D(box.loVect()), ARLIM_3D(box.hiVect()),
			  	          BL_TO_FORTRAN_3D(stateMF[mfi]),
				          ZFILL(dx),&dt);
		}
#ifdef _OPENMP
#pragma omp critical (castro_estdt)
#endif
	      {
		estdt_hydro = std::min(estdt_hydro,dt);
	      }
	    }
	}
	if (diffuse_enth)
	{
#ifdef _OPENMP
#pragma omp parallel
#endif
	    {
	      Real dt = max_dt / cfl;

	      for (MFIter mfi(stateMF,true); mfi.isValid(); ++mfi)
		{
		  const Box& box = mfi.tilebox();
		  ca_estdt_enth_diffusion(ARLIM_3D(box.loVect()), ARLIM_3D(box.hiVect()),
				          BL_TO_FORTRAN_3D(stateMF[mfi]),
				          ZFILL(dx),&dt);
		}
#ifdef _OPENMP
#pragma omp critical (castro_estdt)
#endif
	      {
		estdt_hydro = std::min(estdt_hydro,dt);
	      }
	    }
	}
#endif  // diffusion

#ifdef RADIATION
      }
#endif

       ParallelDescriptor::ReduceRealMin(estdt_hydro);
       estdt_hydro *= cfl;
       if (verbose && ParallelDescriptor::IOProcessor())
           std::cout << "...estimated hydro-limited timestep at level " << level << ": " << estdt_hydro << std::endl;

       // Determine if this is more restrictive than the maximum timestep limiting

       if (estdt_hydro < estdt) {
	 limiter = "hydro";
	 estdt = estdt_hydro;
       }
    }

#ifdef REACTIONS
    MultiFab& S_new = get_new_data(State_Type);
    MultiFab& R_new = get_new_data(Reactions_Type);

    // Dummy value to start with
    Real estdt_burn = max_dt;

    if (do_react) {

        // Compute burning-limited timestep.

#ifdef _OPENMP
#pragma omp parallel
#endif
        {
            Real dt = max_dt;

	    for (MFIter mfi(S_new); mfi.isValid(); ++mfi)
	    {
	        const Box& box = mfi.validbox();

		if (state[State_Type].hasOldData() && state[Reactions_Type].hasOldData()) {

		  MultiFab& S_old = get_old_data(State_Type);
		  MultiFab& R_old = get_old_data(Reactions_Type);

		  ca_estdt_burning(BL_TO_FORTRAN_3D(S_old[mfi]),
                                   BL_TO_FORTRAN_3D(S_new[mfi]),
				   BL_TO_FORTRAN_3D(R_old[mfi]),
				   BL_TO_FORTRAN_3D(R_new[mfi]),
				   ARLIM_3D(box.loVect()),ARLIM_3D(box.hiVect()),
				   ZFILL(dx),&dt_old,&dt);

		} else {

		  ca_estdt_burning(BL_TO_FORTRAN_3D(S_new[mfi]),
                                   BL_TO_FORTRAN_3D(S_new[mfi]),
				   BL_TO_FORTRAN_3D(R_new[mfi]),
				   BL_TO_FORTRAN_3D(R_new[mfi]),
				   ARLIM_3D(box.loVect()),ARLIM_3D(box.hiVect()),
				   ZFILL(dx),&dt_old,&dt);

		}

	    }
#ifdef _OPENMP
#pragma omp critical (castro_estdt_burning)
#endif
	    {
	        estdt_burn = std::min(estdt_burn,dt);
	    }

        }

	ParallelDescriptor::ReduceRealMin(estdt_burn);

	if (verbose && ParallelDescriptor::IOProcessor() && estdt_burn < max_dt) 
	  std::cout << "...estimated burning-limited timestep at level " << level << ": " << estdt_burn << std::endl;

	// Determine if this is more restrictive than the hydro limiting

	if (estdt_burn < estdt) {
	  limiter = "burning";
	  estdt = estdt_burn;
	}
    }
#endif

#ifdef RADIATION
    if (do_radiation) radiation->EstTimeStep(estdt, level);
#endif

    if (verbose && ParallelDescriptor::IOProcessor())
      cout << "Castro::estTimeStep (" << limiter << "-limited) at level " << level << ":  estdt = " << estdt << '\n';

    return estdt;
}

void
Castro::computeNewDt (int                   finest_level,
                      int                   sub_cycle,
                      Array<int>&           n_cycle,
                      const Array<IntVect>& ref_ratio,
                      Array<Real>&          dt_min,
                      Array<Real>&          dt_level,
                      Real                  stop_time,
                      int                   post_regrid_flag)
{
    BL_PROFILE("Castro::computeNewDt()");

    //
    // We are at the start of a coarse grid timecycle.
    // Compute the timesteps for the next iteration.
    //
    if (level > 0)
        return;

    int i;

    Real dt_0 = 1.0e+100;
    int n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        Castro& adv_level = getLevel(i);
        dt_min[i] = adv_level.estTimeStep(dt_level[i]);
    }

    if (fixed_dt <= 0.0)
    {
       if (post_regrid_flag == 1) 
       {
          //
          // Limit dt's by pre-regrid dt
          //
          for (i = 0; i <= finest_level; i++)
          {
              dt_min[i] = std::min(dt_min[i],dt_level[i]);
          }
       } 
       else 
       {
          //
          // Limit dt's by change_max * old dt
          //
          for (i = 0; i <= finest_level; i++)
          {
             if (verbose && ParallelDescriptor::IOProcessor())
                 if (dt_min[i] > change_max*dt_level[i])
                 {
                        cout << "Castro::compute_new_dt : limiting dt at level "
                             << i << '\n';
                        cout << " ... new dt computed: " << dt_min[i]
                             << '\n';
                        cout << " ... but limiting to: "
                             << change_max * dt_level[i] << " = " << change_max
                             << " * " << dt_level[i] << '\n';
                 }
              dt_min[i] = std::min(dt_min[i],change_max*dt_level[i]);
          }
       } 
    }

    //
    // Find the minimum over all levels
    //
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_0 = std::min(dt_0,n_factor*dt_min[i]);
    }

    //
    // Limit dt's by the value of stop_time.
    //
    const Real eps = 0.001*dt_0;
    Real cur_time  = state[State_Type].curTime();
    if (stop_time >= 0.0) {
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }

    n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_level[i] = dt_0/n_factor;
    }
}

void
Castro::computeInitialDt (int                   finest_level,
                          int                   sub_cycle,
                          Array<int>&           n_cycle,
                          const Array<IntVect>& ref_ratio,
                          Array<Real>&          dt_level,
                          Real                  stop_time)
{
    BL_PROFILE("Castro::computeInitialDt()");

    //
    // Grids have been constructed, compute dt for all levels.
    //
    if (level > 0)
        return;

    int i;

    Real dt_0 = 1.0e+100;
    int n_factor = 1;
    ///TODO/DEBUG: This will need to change for optimal subcycling.
    for (i = 0; i <= finest_level; i++)
    {
        dt_level[i] = getLevel(i).initialTimeStep();
        n_factor   *= n_cycle[i];
        dt_0 = std::min(dt_0,n_factor*dt_level[i]);
    }

    //
    // Limit dt's by the value of stop_time.
    //
    const Real eps = 0.001*dt_0;
    Real cur_time  = state[State_Type].curTime();
    if (stop_time >= 0.0) {
        if ((cur_time + dt_0) > (stop_time - eps))
            dt_0 = stop_time - cur_time;
    }

    n_factor = 1;
    for (i = 0; i <= finest_level; i++)
    {
        n_factor *= n_cycle[i];
        dt_level[i] = dt_0/n_factor;
    }
}

void
Castro::post_timestep (int iteration)
{
    BL_PROFILE("Castro::post_timestep()");

    // Pass some information about the state of the simulation to a Fortran module.

    set_amr_info(level, iteration, -1, -1.0, -1.0);

    //
    // Integration cycle on fine level grids is complete
    // do post_timestep stuff here.
    //
    int finest_level = parent->finestLevel();

#ifdef RADIATION
    if (do_radiation && (level < finest_level)) {
        // computeTemp is not needed before or after this call because
        // setup for deferred sync does not touch state, only flux registers.
        radiation->deferred_sync_setup(level);

	if (do_reflux) {
	    radiation->reflux(level);
	    // Since radiation->reflux does not touch the fluid state,
	    // we do need to recompute Temp here.
	}
    }
#endif

#ifdef GRAVITY
    // Check the whole hierarchy before the syncs
    if (do_grav && level == 0 && gravity->test_results_of_solves() == 1 &&
        gravity->get_gravity_type() == "PoissonGrav")
    {
       if (verbose && ParallelDescriptor::IOProcessor())
          std::cout << "before gravity_sync " << std::endl;
       gravity->test_composite_phi(level);
    }
#endif

    if (do_reflux && level < finest_level) {

        MultiFab& S_new_crse = get_new_data(State_Type);

#ifdef GRAVITY
        MultiFab drho_and_drhoU;
        if (do_grav && gravity->get_gravity_type() == "PoissonGrav")  {
           // Define the update to rho and rhoU due to refluxing.
           drho_and_drhoU.define(grids,3+1,0,Fab_allocate);
           MultiFab::Copy(drho_and_drhoU,S_new_crse,Density,0,3+1,0);
           drho_and_drhoU.mult(-1.0);
        }
#endif

        reflux();

        // We need to do this before anything else because refluxing changes the values of coarse cells
        //    underneath fine grids with the assumption they'll be over-written by averaging down
        if (level < finest_level)
           avgDown();

	// Sometimes refluxing can generate zones with rho < small_dens, so let's fix 
	// that now if we need to.

	enforce_min_density(S_new_crse, S_new_crse);

        // This needs to be done after any changes to the state from refluxing.

        normalize_species(S_new_crse);

	// Flush Fortran output

	if (verbose)
	  flush_output();

#ifdef GRAVITY
        if (do_grav && gravity->get_gravity_type() == "PoissonGrav" && gravity->NoSync() == 0)  {

	    MultiFab::Add(drho_and_drhoU, S_new_crse, Density, 0, 3+1, 0);

            MultiFab dphi(grids,1,0,Fab_allocate);
            dphi.setVal(0.);

            gravity->reflux_phi(level,dphi);
                
            // Compute (cross-level) gravity sync based on drho, dphi
            PArray<MultiFab> grad_delta_phi_cc(finest_level-level+1,PArrayManage); 
            for (int lev = level; lev <= finest_level; lev++) {
               grad_delta_phi_cc.set(lev-level,
                                     new MultiFab(getLevel(lev).boxArray(),3,0,Fab_allocate));
               grad_delta_phi_cc[lev-level].setVal(0.);
            }

	    int ncycle = parent->nCycle(level);
	    gravity->gravity_sync(level,finest_level,iteration,ncycle,drho_and_drhoU,dphi,grad_delta_phi_cc);

	    Real dt = parent->dtLevel(level);
	    
            for (int lev = level; lev <= finest_level; lev++)  
            {
              Real dt_lev = parent->dtLevel(lev);
              MultiFab&  S_new_lev = getLevel(lev).get_new_data(State_Type);
              Real cur_time = state[State_Type].curTime();

              const BoxArray& ba = getLevel(lev).boxArray();
              MultiFab grad_phi_cc(ba,3,NUM_GROW,Fab_allocate);
              gravity->get_new_grav_vector(lev,grad_phi_cc,cur_time);

#ifdef _OPENMP
#pragma omp parallel	      
#endif
	      {
		  FArrayBox sync_src;
		  FArrayBox dstate;

		  for (MFIter mfi(S_new_lev,true); mfi.isValid(); ++mfi)
		  {
		      const Box& bx = mfi.tilebox();
		      dstate.resize(bx,3+1);
		      if (lev == level) {
			  dstate.copy(drho_and_drhoU[mfi],bx);
		      } else {
			  dstate.setVal(0.); 
		      }
		      
		      // Compute sync source
#ifdef HYBRID_MOMENTUM
		      sync_src.resize(bx,3+1+3);
#else
		      sync_src.resize(bx,3+1);
#endif
		      ca_syncgsrc(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()),
				  BL_TO_FORTRAN_3D(grad_phi_cc[mfi]),
				  BL_TO_FORTRAN_3D(grad_delta_phi_cc[lev-level][mfi]),
				  BL_TO_FORTRAN_3D(S_new_lev[mfi]),
				  BL_TO_FORTRAN_3D(dstate),
				  BL_TO_FORTRAN_3D(sync_src),
				  dt_lev);

		      // Now multiply the sync source by dt / 2, where dt
		      // is the timestep on the base level, not the refined
		      // levels. Using this level's dt ensures that we correct for
		      // the errors in the previous fine grid timesteps, which
		      // were all slightly incorrect because they didn't have the
		      // contribution from refluxing. Since we do linear interpolation
		      // of gravity in time, the total error sums up so that we
		      // want to use dt / 2 on the base level.
		      
		      sync_src.mult(0.5*dt);
		      S_new_lev[mfi].plus(sync_src,bx,0,Xmom,3);
		      S_new_lev[mfi].plus(sync_src,bx,3,Eden,1);
#ifdef HYBRID_MOMENTUM
		      S_new_lev[mfi].plus(sync_src,bx,4,Rmom,3);
#endif
		  }
	      }
            }

            // Check the whole hierarchy after the syncs
            if (level == 0 && gravity->test_results_of_solves() == 1 && 
                              gravity->get_gravity_type() == "PoissonGrav")
            {
               if (verbose && ParallelDescriptor::IOProcessor())
                  std::cout << "after gravity_sync " << std::endl;
               gravity->test_composite_phi(level);
            }
        }
#endif
    }

    // Sync up the hybrid and linear momenta.

    MultiFab& S_new = get_new_data(State_Type);

#ifdef HYBRID_MOMENTUM
    if (hybrid_hydro) {

#ifdef _OPENMP
#pragma omp parallel
#endif
      for (MFIter mfi(S_new, true); mfi.isValid(); ++mfi) {

	const Box& bx = mfi.tilebox();

	hybrid_update(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()), BL_TO_FORTRAN_3D(S_new[mfi]));

      }

    }
#endif

    if (level < finest_level)
        avgDown();

#ifdef do_problem_post_timestep

    problem_post_timestep();

#endif

    if (level == 0)
    {
        int nstep = parent->levelSteps(0);
	Real dtlev = parent->dtLevel(0);
	Real cumtime = parent->cumTime() + dtlev;

	bool sum_int_test = false;

	if (sum_interval > 0) {

	  if (nstep%sum_interval == 0)
	    sum_int_test = true;

	}

	bool sum_per_test = false;

	if (sum_per > 0.0) {

	  const int num_per_old = floor((cumtime - dtlev) / sum_per);
	  const int num_per_new = floor((cumtime        ) / sum_per);

	  if (num_per_old != num_per_new)
	    sum_per_test = true;

	}

        if (sum_int_test || sum_per_test)
	  sum_integrated_quantities();

#ifdef GRAVITY
        if (moving_center) write_center();
#endif
    }

#ifdef RADIATION
    if (level == 0) {
      if (do_radiation) {
	for (int lev = finest_level; lev >= 0; lev--) {
	  radiation->analytic_solution(lev);
	}
      }
    }

    // diagnostic stuff
    
    if (level == 0) 
      do_energy_diagnostics();
#endif

    // Re-compute temperature after all the other updates.
    computeTemp(S_new);


#ifdef PARTICLES
    if (TracerPC)
    {
	const int ncycle = parent->nCycle(level);
	//
	// Don't redistribute/timestamp on the final subiteration except on the coarsest grid.
	//
	if (iteration < ncycle || level == 0)
	{
	    int ngrow = (level == 0) ? 0 : iteration;

	    TracerPC->Redistribute(false, true, level, ngrow);
            
	    TimestampParticles(ngrow+1);
	}
    }
#endif
}

void
Castro::post_restart ()
{
   BL_PROFILE("Castro::post_restart()");

   Real cur_time = state[State_Type].curTime();

#ifdef PARTICLES
   ParticlePostRestart(parent->theRestartFile());
#endif

#ifdef GRAVITY
    if (do_grav)
    {
        if (level == 0)
        {
            // Passing numpts_1d at level 0 
            int numpts_1d = get_numpts ();
            gravity->set_numpts_in_gravity(numpts_1d);

            for (int lev = 0; lev <= parent->finestLevel(); lev++)
            {
                AmrLevel& this_level  = getLevel(lev);
                Castro& cs_level = getLevel(lev);
                gravity->install_level(lev,&this_level,
                                       cs_level.Volume(),cs_level.Area());
            }

            if (moving_center == 1)
            {
               MultiFab&  S_new = get_new_data(State_Type);
               define_new_center(S_new,cur_time);
            }

            gravity->set_mass_offset(cur_time);

            if ( gravity->get_gravity_type() == "PoissonGrav")
            {
                if (gravity->NoComposite() != 1)
                {
 		   int use_previous_phi = 1;
		   gravity->multilevel_solve_for_new_phi(0,parent->finestLevel(),use_previous_phi);
                   if (gravity->test_results_of_solves() == 1)
                       gravity->test_composite_phi(level);
                }
            }

            if (grown_factor > 1)
                post_grown_restart();
        }
    }
#endif

#ifdef ROTATION
    MultiFab& phirot_new = get_new_data(PhiRot_Type);
    MultiFab& rot_new = get_new_data(Rotation_Type);
    MultiFab& S_new = get_new_data(State_Type);
    if (do_rotation)
      fill_rotation_field(phirot_new, rot_new, S_new, cur_time);
    else {
      phirot_new.setVal(0.0);
      rot_new.setVal(0.0);
    }
#endif    
    
#ifdef DIFFUSION
      // diffusion is a static object, only alloc if not already there
      if (diffusion == 0)
        diffusion = new Diffusion(parent,&phys_bc);

      if (level == 0) 
         for (int lev = 0; lev <= parent->finestLevel(); lev++) {
            AmrLevel& this_level = getLevel(lev);
                Castro& cs_level = getLevel(lev);
            diffusion->install_level(lev,&this_level,
                                     cs_level.Volume(),cs_level.Area());
         }
#endif

    set_special_tagging_flag(cur_time);

    // initialize the Godunov state array used in hydro -- we wait
    // until here so that ngroups is defined (if needed) in
    // rad_params_module
#ifdef RADIATION
    init_godunov_indices_rad();
#else
    init_godunov_indices();
#endif

#ifdef do_problem_post_restart
    problem_post_restart();
#endif

}

void
Castro::postCoarseTimeStep (Real cumtime)
{
    //
    // postCoarseTimeStep() is only called by level 0.
    //
#ifdef GRAVITY
    if (do_grav)
        gravity->set_mass_offset(cumtime, 0);
#endif
}

void
Castro::post_regrid (int lbase,
                     int new_finest)
{
    delete fine_mask;
    fine_mask = 0;

#ifdef PARTICLES
    if (TracerPC && level == lbase) {
	TracerPC->Redistribute(false, false, lbase);
    }
#endif

#ifdef GRAVITY
    if (do_grav)
    {
       const Real cur_time = state[State_Type].curTime();
       if ( (level == lbase) && cur_time > 0.)  
       {
	   if ( gravity->get_gravity_type() == "PoissonGrav" && (gravity->NoComposite() != 1) ) {
	       int use_previous_phi = 1;
	       gravity->multilevel_solve_for_new_phi(level,new_finest,use_previous_phi);
	   }
       }
    }
#endif
}

void
Castro::post_init (Real stop_time)
{
    BL_PROFILE("Castro::post_init()");

    if (level > 0)
        return;

    //
    // Average data down from finer levels
    // so that conserved data is consistent between levels.
    //
    int finest_level = parent->finestLevel();
    for (int k = finest_level-1; k>= 0; k--)
        getLevel(k).avgDown();

    Real cur_time = state[State_Type].curTime();
    
#ifdef GRAVITY
    if (do_grav) {
       if (gravity->get_gravity_type() == "PoissonGrav") {

          // Calculate offset before first multilevel solve.
          gravity->set_mass_offset(cur_time);

          if (gravity->NoComposite() != 1)  {
             gravity->multilevel_solve_for_new_phi(level,finest_level);
             if (gravity->test_results_of_solves() == 1)
                gravity->test_composite_phi(level);
          }
       }
 
       // Make this call just to fill the initial state data.
       for (int k = 0; k <= parent->finestLevel(); k++)
       {
          BoxArray ba = getLevel(k).boxArray();
          MultiFab& grav_new = getLevel(k).get_new_data(Gravity_Type);
          gravity->get_new_grav_vector(k,grav_new,cur_time);
       }
    }
#endif

#ifdef ROTATION
    MultiFab& phirot_new = get_new_data(PhiRot_Type);
    MultiFab& rot_new = get_new_data(Rotation_Type);
    MultiFab& S_new = get_new_data(State_Type);
    if (do_rotation)
      fill_rotation_field(phirot_new, rot_new, S_new, cur_time);
    else {
      phirot_new.setVal(0.0);
      rot_new.setVal(0.0);
    }
#endif
    
#ifdef RADIATION
    if (do_radiation) {
      // The option of whether to do a multilevel initialization is
      // controlled within the radiation class.

      radiation->post_init(level);

      for (int k = finest_level-1; k>= 0; k--) {
        getLevel(k).avgDown(Rad_Type);
      }

      do_energy_diagnostics();

      // re-estimate the initial timestep using the initialized
      // fine-level data and radiation field.
      //if (level == 0)
      //post_init_estDT();
    }
#endif

// Allow the user to define their own post_init functions.

#ifdef do_problem_post_init

    problem_post_init();

#endif

        int nstep = parent->levelSteps(0);
	Real dtlev = parent->dtLevel(0);
	Real cumtime = parent->cumTime();
	if (cumtime != 0.0) cumtime += dtlev;

	bool sum_int_test = false;

	if (sum_interval > 0) {

	  if (nstep%sum_interval == 0)
	    sum_int_test = true;

	}

	bool sum_per_test = false;

	if (sum_per > 0.0) {

	  const int num_per_old = floor((cumtime - dtlev) / sum_per);
	  const int num_per_new = floor((cumtime        ) / sum_per);

	  if (num_per_old != num_per_new)
	    sum_per_test = true;

	}

        if (sum_int_test || sum_per_test)
	  sum_integrated_quantities();

#ifdef GRAVITY
    if (level == 0 && moving_center == 1)
       write_center();
#endif
}

void
Castro::post_grown_restart ()
{
    if (level > 0)
        return;

    int finest_level = parent->finestLevel();
    Real cur_time = state[State_Type].curTime();
    
#ifdef GRAVITY
    if (do_grav) {
       if (gravity->get_gravity_type() == "PoissonGrav") {

          // Calculate offset before first multilevel solve.
          gravity->set_mass_offset(cur_time);

          if (gravity->NoComposite() != 1)  {
             gravity->multilevel_solve_for_new_phi(level,finest_level);
             if (gravity->test_results_of_solves() == 1)
                gravity->test_composite_phi(level);
          }
       }
 
       // Make this call just to fill the initial state data.
       for (int k = 0; k <= parent->finestLevel(); k++)
       {
          MultiFab& grav_new = getLevel(k).get_new_data(Gravity_Type);
          gravity->get_new_grav_vector(k,grav_new,cur_time);
       }
    }
#endif

#ifdef ROTATION
    MultiFab& phirot_new = get_new_data(PhiRot_Type);
    MultiFab& rot_new = get_new_data(Rotation_Type);
    MultiFab& S_new = get_new_data(State_Type);
    if (do_rotation)
      fill_rotation_field(phirot_new, rot_new, S_new, cur_time);
    else {
      phirot_new.setVal(0.0);
      rot_new.setVal(0.0);
    }
#endif    
    
#ifdef RADIATION
    if (do_radiation) {
      // The option of whether to do a multilevel initialization is
      // controlled within the radiation class.

      radiation->post_init(level);

      for (int k = finest_level-1; k>= 0; k--) {
        getLevel(k).avgDown(Rad_Type);
      }

      do_energy_diagnostics();

      // re-estimate the initial timestep using the initialized
      // fine-level data and radiation field.
      //if (level == 0)
      //post_init_estDT();
    }
#endif
}

int
Castro::okToContinue ()
{
    if (level > 0)
        return 1;

    int test = 1;

    if (signalStopJob) {
      test = 0;
      if (ParallelDescriptor::IOProcessor())
	std::cout << " Signalling a stop of the run due to signalStopJob = true." << std::endl;
    }
    else if (parent->dtLevel(0) < dt_cutoff) {
      test = 0;
      if (ParallelDescriptor::IOProcessor())
	std::cout << " Signalling a stop of the run because dt < dt_cutoff." << std::endl;
    }

    return test; 
}

#ifdef AUX_UPDATE
void
Castro::advance_aux(Real time, Real dt)
{
    if (verbose && ParallelDescriptor::IOProcessor())
        std::cout << "... special update for auxiliary variables \n";

    MultiFab&  S_old = get_old_data(State_Type);
    MultiFab&  S_new = get_new_data(State_Type);

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_old,true); mfi.isValid(); ++mfi)
    {
        const Box& box = mfi.tilebox();
        FArrayBox& old_fab = S_old[mfi];
        FArrayBox& new_fab = S_new[mfi];
	void ca_auxupdate(BL_TO_FORTRAN(old_fab),
			  BL_TO_FORTRAN(new_fab),
			  box.loVect(), box.hiVect(),
			  &dt);
    }
}
#endif

#ifdef DIFFUSION
#ifdef TAU
void
Castro::define_tau (MultiFab& grav_vector, Real time)
{
   MultiFab& S_old = get_old_data(State_Type);
   const int ncomp = S_old.nComp();

   const Geometry& fine_geom = parent->Geom(parent->finestLevel());
   const Real*       dx_fine = fine_geom.CellSize();

   for (FillPatchIterator fpi(*this,S_old,NUM_GROW,time,State_Type,Density,ncomp);
                          fpi.isValid();++fpi)
   {
        Box bx(fpi.validbox());
        int i = fpi.index();
        ca_define_tau(bx.loVect(), bx.hiVect(),
		      BL_TO_FORTRAN((*tau_diff)[fpi]),
		      BL_TO_FORTRAN(fpi()),
		      BL_TO_FORTRAN(grav_vector[fpi]),
		      dx_fine);
   }
}
#endif
#endif

void
Castro::reflux ()
{
    BL_PROFILE("Castro::reflux()");

    BL_ASSERT(level<parent->finestLevel());

    const Real strt = ParallelDescriptor::second();

    getFluxReg(level+1).Reflux(get_new_data(State_Type),volume,1.0,0,0,NUM_STATE,geom);

#ifdef RADIATION
    if (Radiation::rad_hydro_combined) {
      getRADFluxReg(level+1).Reflux(get_new_data(Rad_Type),volume,1.0,0,0,Radiation::nGroups,geom);
    }
#endif

    if (verbose)
    {
        const int IOProc = ParallelDescriptor::IOProcessorNumber();
        Real      end    = ParallelDescriptor::second() - strt;

#ifdef BL_LAZY
	Lazy::QueueReduction( [=] () mutable {
#endif
        ParallelDescriptor::ReduceRealMax(end,IOProc);
        if (ParallelDescriptor::IOProcessor())
            std::cout << "Castro::reflux() at level " << level << " : time = " << end << std::endl;
#ifdef BL_LAZY
	});
#endif
    }
}

void
Castro::avgDown ()
{
    BL_PROFILE("Castro::avgDown()");

  if (level == parent->finestLevel()) return;

  avgDown(State_Type);

#ifdef GRAVITY
  avgDown(Gravity_Type);
  avgDown(PhiGrav_Type);
#endif

#ifdef ROTATION
  avgDown(Rotation_Type);
  avgDown(PhiRot_Type);
#endif

  avgDown(Source_Type);
  
#ifdef REACTIONS
  avgDown(Reactions_Type);
#endif

#ifdef LEVELSET
  avgDown(LS_State_Type);
#endif

#ifdef RADIATION
  if (do_radiation) {
    avgDown(Rad_Type);
  }
#endif

}

void
Castro::normalize_species (MultiFab& S_new)
{
    int ng = S_new.nGrow();

#ifdef _OPENMP
#pragma omp parallel
#endif    
    for (MFIter mfi(S_new,true); mfi.isValid(); ++mfi)
    {
       const Box& bx = mfi.growntilebox(ng);
       ca_normalize_species(BL_TO_FORTRAN_3D(S_new[mfi]),ARLIM_3D(bx.loVect()),ARLIM_3D(bx.hiVect()));
    }
}

void
Castro::enforce_consistent_e (MultiFab& S)
{

#ifdef _OPENMP
#pragma omp parallel
#endif    
    for (MFIter mfi(S,true); mfi.isValid(); ++mfi)
    {
        const Box& box     = mfi.tilebox();
        const int* lo      = box.loVect();
        const int* hi      = box.hiVect();

        ca_enforce_consistent_e(ARLIM_3D(lo), ARLIM_3D(hi), BL_TO_FORTRAN_3D(S[mfi]));
    }
}

Real
Castro::enforce_min_density (MultiFab& S_old, MultiFab& S_new)
{

    // This routine sets the density in S_new to be larger than the density floor.
    // Note that it will operate everywhere on S_new, including ghost zones.
    // S_old is present so that, after the hydro call, we know what the old density
    // was so that we have a reference for comparison. If you are calling it elsewhere
    // and there's no meaningful reference state, just pass in the same MultiFab twice.

    // The return value is the the negative fractional change in the state that has the
    // largest magnitude. If there is no reference state, this is meaningless.

    Real dens_change = 1.e0;

    Real mass_added = 0.0;
    Real eint_added = 0.0;
    Real eden_added = 0.0;

#ifdef _OPENMP
#pragma omp parallel reduction(min:dens_change)
#endif
    for (MFIter mfi(S_new, true); mfi.isValid(); ++mfi) {

	FArrayBox& stateold = S_old[mfi];
	FArrayBox& statenew = S_new[mfi];
	FArrayBox& vol      = volume[mfi];

	enforce_minimum_density(stateold.dataPtr(), ARLIM_3D(stateold.loVect()), ARLIM_3D(stateold.hiVect()),
				statenew.dataPtr(), ARLIM_3D(statenew.loVect()), ARLIM_3D(statenew.hiVect()),
				vol.dataPtr(), ARLIM_3D(vol.loVect()), ARLIM_3D(vol.hiVect()),
				ARLIM_3D(statenew.loVect()), ARLIM_3D(statenew.hiVect()),
				&mass_added, &eint_added, &eden_added, &dens_change,
				&verbose);

    }

    if (print_energy_diagnostics)
    {

        Real foo[3] = {mass_added, eint_added, eden_added};

#ifdef BL_LAZY
        Lazy::QueueReduction( [=] () mutable {
#endif
	    ParallelDescriptor::ReduceRealSum(foo, 3, ParallelDescriptor::IOProcessorNumber());

	    if (ParallelDescriptor::IOProcessor())
	    {
	        mass_added = foo[0];
		eint_added = foo[1];
		eden_added = foo[2];

		if (std::abs(mass_added) != 0.0)
	        {
		  std::cout << "   Mass added from negative density correction : " <<
				mass_added << std::endl;
		  std::cout << "(rho e) added from negative density correction : " <<
				eint_added << std::endl;
		  std::cout << "(rho E) added from negative density correction : " <<
				eden_added << std::endl;
	        }
	    }
#ifdef BL_LAZY
        });
#endif

    }

    return dens_change;

}

void
Castro::avgDown (int state_indx)
{
    BL_PROFILE("Castro::avgDown(state_indx)");

    if (level == parent->finestLevel()) return;

    Castro& fine_lev = getLevel(level+1);

    const Geometry& fgeom = fine_lev.geom;
    const Geometry& cgeom =          geom;

    MultiFab&  S_crse   = get_new_data(state_indx);
    MultiFab&  S_fine   = fine_lev.get_new_data(state_indx);

    BoxLib::average_down(S_fine, S_crse,
			 fgeom, cgeom,
			 0, S_fine.nComp(), fine_ratio);
}

void
Castro::allocOldData ()
{
    for (int k = 0; k < NUM_STATE_TYPE; k++)
        state[k].allocOldData();
}

void
Castro::removeOldData()
{
    AmrLevel::removeOldData();
}

void
Castro::errorEst (TagBoxArray& tags,
                  int          clearval,
                  int          tagval,
                  Real         time,
                  int          n_error_buf,
                  int          ngrow)
{
    BL_PROFILE("Castro::errorEst()");

    set_amr_info(level, -1, -1, -1.0, -1.0);    
    
    const int*  domain_lo = geom.Domain().loVect();
    const int*  domain_hi = geom.Domain().hiVect();
    const Real* dx        = geom.CellSize();
    const Real* prob_lo   = geom.ProbLo();

    for (int j = 0; j < err_list.size(); j++)
    {
        MultiFab* mf = derive(err_list[j].name(), time, err_list[j].nGrow());

        BL_ASSERT(!(mf == 0));

#ifdef _OPENMP
#pragma omp parallel
#endif
	{
	    Array<int>  itags;

	    for (MFIter mfi(*mf,true); mfi.isValid(); ++mfi)
	    {
		// FABs
		FArrayBox&  datfab  = (*mf)[mfi];
		TagBox&     tagfab  = tags[mfi];

		// tile box
		const Box&  tilebx  = mfi.tilebox();

		// physical tile box
		const RealBox& pbx  = RealBox(tilebx,geom.CellSize(),geom.ProbLo());

		//fab box
		const Box&  datbox  = datfab.box();

		// We cannot pass tagfab to Fortran becuase it is BaseFab<char>.
		// So we are going to get a temporary integer array.
		tagfab.get_itags(itags, tilebx);

		// data pointer and index space
		int*        tptr    = itags.dataPtr();
		const int*  tlo     = tilebx.loVect();
		const int*  thi     = tilebx.hiVect();
		//
		const int*  lo      = tlo;
		const int*  hi      = thi;
		//
		const Real* xlo     = pbx.lo();
		//
		Real*       dat     = datfab.dataPtr();
		const int*  dlo     = datbox.loVect();
		const int*  dhi     = datbox.hiVect();
		const int   ncomp   = datfab.nComp();
		
		err_list[j].errFunc()(tptr, tlo, thi, &tagval,
				      &clearval, dat, dlo, dhi,
				      lo,hi, &ncomp, domain_lo, domain_hi,
				      dx, xlo, prob_lo, &time, &level);
		//
		// Now update the tags in the TagBox.
		//
                tagfab.tags_and_untags(itags, tilebx);
	    }
	}

        delete mf;
    }

    // Now we'll tag any user-specified zones using the full state array.

    MultiFab& S_new = get_new_data(State_Type);


#ifdef _OPENMP
#pragma omp parallel
#endif
    {
        Array<int>  itags;

	for (MFIter mfi(S_new,true); mfi.isValid(); ++mfi)
	{
	    // tile box
	    const Box&  tilebx  = mfi.tilebox();

            TagBox&     tagfab  = tags[mfi];

	    // We cannot pass tagfab to Fortran becuase it is BaseFab<char>.
	    // So we are going to get a temporary integer array.
	    tagfab.get_itags(itags, tilebx);

            // data pointer and index space
	    int*        tptr    = itags.dataPtr();
	    const int*  tlo     = tilebx.loVect();
	    const int*  thi     = tilebx.hiVect();

#ifdef DIMENSION_AGNOSTIC
	    set_problem_tags(tptr,  ARLIM_3D(tlo), ARLIM_3D(thi),
			     BL_TO_FORTRAN_3D(S_new[mfi]),
			     &tagval, &clearval, 
			     ARLIM_3D(tilebx.loVect()), ARLIM_3D(tilebx.hiVect()), 
			     ZFILL(dx), ZFILL(prob_lo), &time, &level);
#else	    
	    set_problem_tags(tptr,  ARLIM(tlo), ARLIM(thi),
			     BL_TO_FORTRAN(S_new[mfi]),
			     &tagval, &clearval, 
			     tilebx.loVect(), tilebx.hiVect(), 
			     dx, prob_lo, &time, &level);
#endif
	    
	    //
	    // Now update the tags in the TagBox.
	    //
            tagfab.tags_and_untags(itags, tilebx);
	}
    }
}

MultiFab*
Castro::derive (const std::string& name,
                Real           time,
                int            ngrow)
{
#ifdef NEUTRINO
  if (name.substr(0,4) == "Neut") {
    // Extract neutrino energy group number from name string and
    // pass to fortran so that derive will have access to it:
    int is = atoi(name.c_str() + name.find('s') + 1);
    int ig = atoi(name.c_str() + name.find('g') + 1);

    BL_ASSERT(is < Radiation::nNeutrinoSpecies);
    for (int n = 0; n < is; n++) {
      ig += Radiation::nNeutrinoGroups[n];
    }

    ca_setgroup(ig);
  }
#endif

#ifdef PARTICLES
  return ParticleDerive(name,time,ngrow);
#else
   return AmrLevel::derive(name,time,ngrow);
#endif
}

void
Castro::derive (const std::string& name,
                Real           time,
                MultiFab&      mf,
                int            dcomp)
{
#ifdef NEUTRINO
  if (name.substr(0,4) == "Neut") {
    // Extract neutrino energy group number from name string and
    // pass to fortran so that derive will have access to it:
    int is = atoi(name.c_str() + name.find('s') + 1);
    int ig = atoi(name.c_str() + name.find('g') + 1);

    BL_ASSERT(is < Radiation::nNeutrinoSpecies);
    for (int n = 0; n < is; n++) {
      ig += Radiation::nNeutrinoGroups[n];
    }

    ca_setgroup(ig);
  }
#endif

    AmrLevel::derive(name,time,mf,dcomp);
}

void
Castro::network_init ()
{
   ca_network_init();
}

void
Castro::extern_init ()
{
  // initialize the external runtime parameters -- these will
  // live in the probin

  if (ParallelDescriptor::IOProcessor()) {
    std::cout << "reading extern runtime parameters ..." << std::endl;
  }

  int probin_file_length = probin_file.length();
  Array<int> probin_file_name(probin_file_length);

  for (int i = 0; i < probin_file_length; i++)
    probin_file_name[i] = probin_file[i];

  ca_extern_init(probin_file_name.dataPtr(),&probin_file_length);
}

void
Castro::reset_internal_energy(MultiFab& S_new)
{
    Real sum  = 0.;
    Real sum0 = 0.;

    if (parent->finestLevel() == 0 && print_energy_diagnostics)
    {
        // Pass in the multifab and the component
        sum0 = volWgtSumMF(&S_new,Eden,true);
    }

    // Synchronize (rho e) and (rho E) so they are consistent with each other
#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(S_new,true); mfi.isValid(); ++mfi)
    {
        const Box& bx = mfi.tilebox();

        reset_internal_e(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()), 
                         BL_TO_FORTRAN_3D(S_new[mfi]), 
			 print_fortran_warnings);
    }

    // Flush Fortran output

    if (verbose)
      flush_output();

    if (parent->finestLevel() == 0 && print_energy_diagnostics)
    {
        // Pass in the multifab and the component
        sum = volWgtSumMF(&S_new,Eden,true);
#ifdef BL_LAZY
	Lazy::QueueReduction( [=] () mutable {
#endif
	ParallelDescriptor::ReduceRealSum(sum0);
	ParallelDescriptor::ReduceRealSum(sum);
        if (ParallelDescriptor::IOProcessor() && std::abs(sum-sum0) > 0)
            std::cout << "(rho E) added from reset terms                 : " << sum-sum0 << " out of " << sum0 << std::endl;
#ifdef BL_LAZY
	});
#endif
    }
}

void
Castro::computeTemp(MultiFab& State)
{
#ifdef RADIATION

  int resetEint = 1;
  radiation->computeTemp(State, resetEint);

#else
    reset_internal_energy(State);

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(State,true); mfi.isValid(); ++mfi)
    { 
      const Box& bx = mfi.tilebox();
      compute_temp(ARLIM_3D(bx.loVect()),ARLIM_3D(bx.hiVect()),BL_TO_FORTRAN_3D(State[mfi]));
    }

#endif
}

void
Castro::set_special_tagging_flag(Real time)
{
   if (!do_special_tagging) return;

   MultiFab& S_new = get_new_data(State_Type);
   Real max_den = S_new.norm0(Density);

   int flag_was_changed = 0;
   ca_set_special_tagging_flag(max_den,&flag_was_changed);
   if (ParallelDescriptor::IOProcessor()) {
      if (flag_was_changed == 1) {
        std::ofstream os("Bounce_time",std::ios::out);
        os << "T_Bounce " << time << std::endl;
        os.close();
      } 
   } 
}

int
Castro::get_numpts ()
{
     int numpts_1d;

     Box bx(geom.Domain());
     long nx = bx.size()[0];

#if (BL_SPACEDIM == 1)
     numpts_1d = nx;
#elif (BL_SPACEDIM == 2)
     long ny = bx.size()[1];
     Real ndiagsq = Real(nx*nx + ny*ny);
     numpts_1d = int(sqrt(ndiagsq))+2*NUM_GROW;  
#elif (BL_SPACEDIM == 3)
     long ny = bx.size()[1];
     long nz = bx.size()[2];
     Real ndiagsq = Real(nx*nx + ny*ny + nz*nz);
     numpts_1d = int(sqrt(ndiagsq))+2*NUM_GROW;
#endif 

     if (verbose && ParallelDescriptor::IOProcessor())
         std::cout << "Castro::numpts_1d at level  " << level << " is " << numpts_1d << std::endl;

     return numpts_1d;
}

void
Castro::add_force_to_sources(MultiFab& force, MultiFab& sources, MultiFab& state)
{
   int ng = state.nGrow();

   MultiFab temp_MF(grids,1,ng,Fab_allocate);

   for (int i = 0; i < 3; i++) {

     MultiFab::Copy(temp_MF,force,i,0,1,ng);

     // Multiply the force by the density to put it in conservative form.

     MultiFab::Multiply(temp_MF,state,Density,0,1,ng);
     MultiFab::Add(sources,temp_MF,0,Xmom+i,1,ng);

     MultiFab::Copy(temp_MF,force,i,0,1,ng);

     // Add corresponding energy source term (v . src).

     MultiFab::Multiply(temp_MF,state,Xmom+i,0,1,ng);
     MultiFab::Add(sources,temp_MF,0,Eden,1,ng);

   }

}

void
Castro::apply_source_to_state(MultiFab& state, MultiFab& source, Real dt)
{

  BL_ASSERT(source.nGrow() >= state.nGrow());

  MultiFab::Saxpy(state, dt, source, 0, 0, NUM_STATE, state.nGrow());

}

void
Castro::time_center_source_terms(MultiFab& S_new, MultiFab& src_old, MultiFab &src_new, Real dt)
{
  BL_PROFILE("Castro::time_center_source_terms()");

  // Subtract off half of the old source term, and add half of the new.

  src_old.mult(-0.5*dt);
  src_new.mult( 0.5*dt);

  MultiFab::Add(S_new,src_old,0,0,S_new.nComp(),0);
  MultiFab::Add(S_new,src_new,0,0,S_new.nComp(),0);

  // Return the source terms to their original form.

  if (dt > 0.0) {
    src_old.mult(1.0/(-0.5*dt));
    src_new.mult(1.0/( 0.5*dt));
  }
}

void
Castro::make_radial_data(int is_new)
{
#if (BL_SPACEDIM > 1)

 // We only call this for level = 0
   BL_ASSERT(level == 0);
   
   int numpts_1d = get_numpts();

   Array<Real> radial_vol(numpts_1d,0);

   const Real* dx = geom.CellSize();
   Real  dr = dx[0];

   if (is_new == 1) {
      MultiFab& S = get_new_data(State_Type);
      int nc = S.nComp();
      Array<Real> radial_state(numpts_1d*nc,0);
      for (MFIter mfi(S); mfi.isValid(); ++mfi)
      {
         Box bx(mfi.validbox());
         ca_compute_avgstate(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()),ZFILL(dx),&dr,&nc,
			     BL_TO_FORTRAN_3D(     S[mfi]),radial_state.dataPtr(),
			     BL_TO_FORTRAN_3D(volume[mfi]),radial_vol.dataPtr(),
			     ZFILL(geom.ProbLo()),&numpts_1d);
      }

      ParallelDescriptor::ReduceRealSum(radial_vol.dataPtr(),numpts_1d);
      ParallelDescriptor::ReduceRealSum(radial_state.dataPtr(),numpts_1d*nc);

      int first = 0; 
      int np_max = 0; 
      for (int i = 0; i < numpts_1d; i++) {
         if (radial_vol[i] > 0.) 
         {
            for (int j = 0; j < nc; j++) {
              radial_state[nc*i+j] /= radial_vol[i];
            }
         } else if (first == 0) {
            np_max = i; 
            first  = 1; 
         }
      }

      Array<Real> radial_state_short(np_max*nc,0);

      for (int i = 0; i < np_max; i++) {
         for (int j = 0; j < nc; j++) {
           radial_state_short[nc*i+j] = radial_state[nc*i+j];
         }
      }

      Real new_time = state[State_Type].curTime();
      set_new_outflow_data(radial_state_short.dataPtr(),&new_time,&np_max,&nc);
   }
   else
   {
      MultiFab& S = get_old_data(State_Type);
      int nc = S.nComp();
      Array<Real> radial_state(numpts_1d*nc,0);
      for (MFIter mfi(S); mfi.isValid(); ++mfi)
      {
         Box bx(mfi.validbox());
         ca_compute_avgstate(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()),ZFILL(dx),&dr,&nc,
			     BL_TO_FORTRAN_3D(     S[mfi]),radial_state.dataPtr(),
			     BL_TO_FORTRAN_3D(volume[mfi]),radial_vol.dataPtr(),
			     ZFILL(geom.ProbLo()),&numpts_1d);
      }

      ParallelDescriptor::ReduceRealSum(radial_vol.dataPtr(),numpts_1d);
      ParallelDescriptor::ReduceRealSum(radial_state.dataPtr(),numpts_1d*nc);

      int first = 0; 
      int np_max = 0; 
      for (int i = 0; i < numpts_1d; i++) {
         if (radial_vol[i] > 0.) 
         {
            for (int j = 0; j < nc; j++) {
              radial_state[nc*i+j] /= radial_vol[i];
            }
         } else if (first == 0) {
            np_max = i; 
            first  = 1; 
         }
      }

      Array<Real> radial_state_short(np_max*nc,0);

      for (int i = 0; i < np_max; i++) {
         for (int j = 0; j < nc; j++) {
           radial_state_short[nc*i+j] = radial_state[nc*i+j];
         }
      }

      Real old_time = state[State_Type].prevTime();
      set_old_outflow_data(radial_state_short.dataPtr(),&old_time,&np_max,&nc);
   }

#endif
}

#ifdef GRAVITY
void
Castro::define_new_center(MultiFab& S, Real time)
{
    Real center[3];
    const Real* dx = geom.CellSize();

    IntVect max_index = S.maxIndex(Density,0);
    Box bx(max_index,max_index);
    bx.grow(1);
    BoxArray ba(bx);
    MultiFab mf(ba,1,0);
    int owner = mf.DistributionMap()[0];

    // Define a cube 3-on-a-side around the point with the maximum density
    FillPatch(*this,mf,0,time,State_Type,Density,1);

    int mi[BL_SPACEDIM];
    for (int i = 0; i < BL_SPACEDIM; i++) mi[i] = max_index[i];

    // Find the position of the "center" by interpolating from data at cell centers
    for (MFIter mfi(mf); mfi.isValid(); ++mfi) 
    {
        find_center(mf[mfi].dataPtr(),&center[0],ARLIM_3D(mi),ZFILL(dx),ZFILL(geom.ProbLo()));
    }
    // Now broadcast to everyone else.
    ParallelDescriptor::Bcast(&center[0], BL_SPACEDIM, owner);

    // Make sure if R-Z that center stays exactly on axis
    if ( Geometry::IsRZ() ) center[0] = 0;  

    set_center(ZFILL(center));
}

void
Castro::write_center ()
{
    int ndatalogs = parent->NumDataLogs();
 
    if ( (moving_center==1) && (ndatalogs > 0) && ParallelDescriptor::IOProcessor())
    {
       std::ostream& data_logc = parent->DataLog(0);
 
       int nstep = parent->levelSteps(0);
       Real time = state[State_Type].curTime();

       Real center[3];
       get_center(center);
 
       if (time == 0.0) {
           data_logc << std::setw( 8) <<  "   nstep";
           data_logc << std::setw(14) <<  "         time  ";
           data_logc << std::setw(14) <<  "         center" << std::endl;;
       }
 
           data_logc << std::setw( 8) <<  nstep;
           data_logc << std::setw(14) <<  std::setprecision(6) <<  time;
           data_logc << std::setw(14) <<  std::setprecision(6) << center[0];
#if (BL_SPACEDIM >= 2)
           data_logc << std::setw(14) <<  std::setprecision(6) << center[1];
#endif
#if (BL_SPACEDIM == 3)
           data_logc << std::setw(14) <<  std::setprecision(6) << center[2];
#endif
           data_logc << std::endl;
    }
}

#endif


Real
Castro::getCPUTime()
{
  
  int numCores = ParallelDescriptor::NProcs();
#ifdef _OPENMP
  numCores = numCores*omp_get_max_threads();
#endif    

  Real T = numCores*(ParallelDescriptor::second() - startCPUTime) + 
    previousCPUTimeUsed;

  return T;
}


MultiFab*
Castro::build_fine_mask()
{
    BL_ASSERT(level > 0); // because we are building a mask for the coarser level

    if (fine_mask != 0) return fine_mask;

    BoxArray baf = parent->boxArray(level);
    baf.coarsen(crse_ratio);

    const BoxArray& bac = parent->boxArray(level-1);
    fine_mask = new MultiFab(bac,1,0);
    fine_mask->setVal(1.0);
    
#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(*fine_mask); mfi.isValid(); ++mfi)
    {
        FArrayBox& fab = (*fine_mask)[mfi];

	const std::vector< std::pair<int,Box> >& isects = baf.intersections(fab.box());

	for (int ii = 0; ii < isects.size(); ++ii)
	{
	    fab.setVal(0.0,isects[ii].second,0);
	}
    }

    return fine_mask;
}

const iMultiFab&
Castro::build_interior_boundary_mask (int ng)
{
    for (int i = 0; i < ib_mask.size(); ++i)
    {
	if (ib_mask[i].nGrow() == ng) {
	    return ib_mask[i];
	}
    }

    //  If we got here, we need to build a new one

    if (ib_mask.size() == 0) {
	ib_mask.resize(0,PArrayManage);
    }

    iMultiFab& imf = *(ib_mask.push_back(new iMultiFab(grids, 1, ng)));

    const Box& the_domain = geom.Domain();

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(imf); mfi.isValid(); ++mfi)
    {
        IArrayBox& fab = imf[mfi];
	fab.setVal(1);

	if (ng > 0)
	{
	    const Box& bx = fab.box();	
	    const std::vector< std::pair<int,Box> >& isects = grids.intersections(bx);
	    for (int ii = 0; ii < isects.size(); ii++)
	    {
		fab.setVal(0,isects[ii].second,0);
	    }

	    if (Geometry::isAnyPeriodic() && !the_domain.contains(bx))
	    {
		Array<IntVect> pshifts(26);
		geom.periodicShift(the_domain, bx, pshifts);
		for (Array<IntVect>::const_iterator pit = pshifts.begin();
		     pit != pshifts.end(); ++pit)
	        {
		    const IntVect& iv   = *pit;
		    const Box&     shft = bx + iv;

		    const std::vector< std::pair<int,Box> >& isects = grids.intersections(shft);
		    for (int ii = 0; ii < isects.size(); ii++)
		    {
			const Box& dst = isects[ii].second - iv;
			fab.setVal(0,dst,0);
		    }		
		}
	    }

	    fab.setVal(1,mfi.validbox(),0);
	}
    }

    return imf;
}

// Fill a version of the state with ng ghost zones from the state data.

void
Castro::expand_state(MultiFab& Sborder, Real time, int ng)
{
    BL_ASSERT(Sborder.nGrow() >= ng);

    AmrLevel::FillPatch(*this,Sborder,ng,time,State_Type,0,NUM_STATE);

    // It's possible for interpolation to generate negative/small densities.

    enforce_min_density(Sborder, Sborder);

    // It's also possible for interpolation to create very small negative values for species.

    normalize_species(Sborder);

}

void
Castro::construct_old_sources(int amr_iteration, int amr_ncycle, int sub_iteration, int sub_ncycle, Real time, Real dt)
{

    for (int n = 0; n < num_src; ++n)
	old_sources[n].setVal(0.0);

    sources_for_hydro->setVal(0.0,NUM_GROW);

    if (do_sponge)
        construct_old_sponge_source(time, dt);

    if (add_ext_src)
        construct_old_ext_source(time, dt);

#ifdef GRAVITY
    construct_old_gravity(amr_iteration, amr_ncycle, sub_iteration, sub_ncycle, time);
    construct_old_gravity_source(time, dt);
#endif

#ifdef DIFFUSION
    construct_old_diff_source(time, dt);
#endif

#ifdef HYBRID_MOMENTUM
    construct_old_hybrid_source(time, dt);
#endif

#ifdef ROTATION
    construct_old_rotation(amr_iteration, amr_ncycle, sub_iteration, sub_ncycle, time);
    construct_old_rotation_source(time, dt);
#endif

    // If we're doing SDC, time-center the source term (using the current iteration's old sources
    // and the last iteration's new sources).

#ifdef SDC

    sources_for_hydro->mult(0.5, NUM_GROW);

    for (int n = 0; n < num_src; ++n)
        MultiFab::Saxpy(*sources_for_hydro, 0.5, new_sources[n], 0, 0, NUM_STATE, 0);

    BoxLib::fill_boundary(*sources_for_hydro, geom);

#endif

}

void
Castro::construct_new_sources(int amr_iteration, int amr_ncycle, int sub_iteration, int sub_ncycle, Real time, Real dt)
{

    // Update the dSdt MultiFab. Get rid of the dt/2 * dS/dt that
    // we added from the last timestep, then subtract the old sources
    // data to get the first half of the update, which is needed for
    // the next calculation of dS/dt.

    if (source_term_predictor == 1) {
      MultiFab& dSdt_new = get_new_data(Source_Type);
      MultiFab::Subtract(*sources_for_hydro,dSdt_new,0,0,NUM_STATE,NUM_GROW);
      dSdt_new.setVal(0.0, NUM_GROW);
      MultiFab::Subtract(dSdt_new,*sources_for_hydro,0,0,NUM_STATE,0);
    }

    for (int n = 0; n < num_src; ++n) {
	new_sources[n].setVal(0.0);
    }

    sources_for_hydro->setVal(0.0,NUM_GROW);

    hydro_source->setVal(0.0);

    if (do_sponge)
        construct_new_sponge_source(time, dt);

    if (add_ext_src)
       construct_new_ext_source(time, dt);

#ifdef HYBRID_MOMENTUM
    construct_new_hybrid_source(time, dt);
#endif

#ifdef DIFFUSION
    NewTempDiffTerm = OldTempDiffTerm;
    NewSpecDiffTerm = OldSpecDiffTerm;
    NewViscousTermforMomentum = OldViscousTermforMomentum;
    NewViscousTermforEnergy   = OldViscousTermforEnergy;

    construct_new_diff_source(time, dt);
#endif

#ifdef GRAVITY
    construct_new_gravity(amr_iteration, amr_ncycle, sub_iteration, sub_ncycle, time);
    construct_new_gravity_source(time, dt);
#endif

#ifdef ROTATION
    construct_new_rotation(amr_iteration, amr_ncycle, sub_iteration, sub_ncycle, time);
    construct_new_rotation_source(time, dt);
#endif
}

void
Castro::check_for_nan(MultiFab& state)
{
    if (state.contains_nan(Density,state.nComp(),0,true))
    {
        for (int i = 0; i < state.nComp(); i++)
        {
	if (state.contains_nan(Density + i, 1, 0,true))
            {
                std::string abort_string = std::string("State has NaNs in the ") + desc_lst[State_Type].name(i) + std::string(" component::advance_hydro()");
                BoxLib::Abort(abort_string.c_str());
            }
        }
    }
}

// Convert a MultiFab with conservative state data u to a primitive MultiFab q.

void
Castro::cons_to_prim(MultiFab& u, MultiFab& q)
{

    BL_ASSERT(u.nComp() == NUM_STATE);
    BL_ASSERT(q.nComp() == QVAR);
    BL_ASSERT(q.nGrow() >= u.nGrow());

    int ng = u.nGrow();

#ifdef _OPENMP
#pragma omp parallel
#endif
    for (MFIter mfi(u, true); mfi.isValid(); ++mfi) {

        const Box& bx = mfi.growntilebox(ng);

	ctoprim(ARLIM_3D(bx.loVect()), ARLIM_3D(bx.hiVect()),
		u[mfi].dataPtr(), ARLIM_3D(u[mfi].loVect()), ARLIM_3D(u[mfi].hiVect()),
		q[mfi].dataPtr(), ARLIM_3D(q[mfi].loVect()), ARLIM_3D(q[mfi].hiVect()));

    }

}

// Obtain the sum of all source terms.

void
Castro::sum_of_sources(MultiFab& source)
{

  int ng = source.nGrow();

  source.setVal(0.0, ng);

  for (int n = 0; n < num_src; ++n)
      MultiFab::Add(source, old_sources[n], 0, 0, NUM_STATE, ng);

  MultiFab::Add(source, *hydro_source, 0, 0, NUM_STATE, ng);

  for (int n = 0; n < num_src; ++n)
      MultiFab::Add(source, new_sources[n], 0, 0, NUM_STATE, ng);

}

// Obtain the effective source term due to reactions on the primitive variables.

#ifdef REACTIONS
#ifdef SDC
void
Castro::get_react_source_prim(MultiFab& react_src, Real dt)
{
    MultiFab& S_old = get_old_data(State_Type);
    MultiFab& S_new = get_new_data(State_Type);

    // Carries the contribution of all non-reacting source terms.

    MultiFab A(grids, NUM_STATE, 0, Fab_allocate);

    sum_of_sources(A);

    // Compute the state that has effectively only been updated with advection.

    MultiFab S_noreact(grids, NUM_STATE, 0, Fab_allocate);

    MultiFab::Copy(S_noreact, S_old, 0, 0, NUM_STATE, S_noreact.nGrow());
    MultiFab::Saxpy(S_noreact, dt, A, 0, 0, NUM_STATE, S_noreact.nGrow());

    // Compute its primitive counterpart.

    MultiFab q_noreact(grids, QVAR, 0, Fab_allocate);

    cons_to_prim(S_noreact, q_noreact);

    // Compute the primitive version of the old state.

    MultiFab q_old(grids, QVAR, 0, Fab_allocate);

    cons_to_prim(S_old, q_old);

    // Compute the effective advective update on the primitive state.

    MultiFab A_prim(grids, QVAR, 0, Fab_allocate);

    A_prim.setVal(0.0);

    if (dt > 0.0) {
        MultiFab::Saxpy(A_prim, -1.0 / dt, q_noreact, 0, 0, QVAR, 0);
	MultiFab::Saxpy(A_prim,  1.0 / dt, q_old,     0, 0, QVAR, 0);
    }

    // Compute the primitive version of the new state.

    MultiFab q_new(grids, QVAR, 0, Fab_allocate);

    cons_to_prim(S_new, q_new);

    // Compute the reaction source term.

    react_src.setVal(0.0);

    if (dt > 0.0) {
        MultiFab::Saxpy(react_src,  1.0 / dt, q_new, 0, 0, QVAR, 0);
	MultiFab::Saxpy(react_src, -1.0 / dt, q_old, 0, 0, QVAR, 0);
    }

    MultiFab::Add(react_src, A_prim, 0, 0, QVAR, 0);

    // Now fill all of the ghost zones.

    BoxLib::fill_boundary(react_src, geom);
}
#endif
#endif
