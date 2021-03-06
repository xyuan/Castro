module bc_fill_module

  use amrex_fort_module, only: rt => amrex_real
  use meth_params_module, only: NVAR
  use prob_params_module, only: dim

  implicit none

  include 'AMReX_bc_types.fi'

  public

contains

  ! All subroutines in this file must be threadsafe because they are called
  ! inside OpenMP parallel regions.

  subroutine hypfill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    use bc_ext_fill_module, only: ext_fill

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: adv_lo(3), adv_hi(3)
    integer,  intent(in   ) :: bc(3,2,NVAR)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_lo(1):adv_hi(1),adv_lo(2):adv_hi(2),adv_lo(3):adv_hi(3),NVAR)

    call amrex_filccn(adv_lo, adv_hi, adv, adv_lo, adv_hi, NVAR, domlo, domhi, delta, xlo, bc)

    ! process the external BCs here
    call ext_fill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine hypfill


  subroutine ca_hypfill(adv,adv_lo,adv_hi,domlo,domhi,delta,xlo,time,bc) &
                        bind(C, name="ca_hypfill")

    use meth_params_module, only: NVAR

    implicit none

    integer,  intent(in   ) :: adv_lo(3), adv_hi(3)
    integer,  intent(in   ) :: bc(dim,2,NVAR)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_lo(1):adv_hi(1),adv_lo(2):adv_hi(2),adv_lo(3):adv_hi(3),NVAR)

    call hypfill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_hypfill


  subroutine denfill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn
    use bc_ext_fill_module, only: ext_denfill

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: adv_lo(3), adv_hi(3)
    integer,  intent(in   ) :: bc(3,2,1)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_lo(1):adv_hi(1),adv_lo(2):adv_hi(2),adv_lo(3):adv_hi(3))

    call amrex_filccn(adv_lo, adv_hi, adv, adv_lo, adv_hi, 1, domlo, domhi, delta, xlo, bc)

    ! process the external BCs here
    call ext_denfill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine denfill


  subroutine ca_denfill(adv,adv_lo,adv_hi,domlo,domhi,delta,xlo,time,bc) &
                        bind(C, name="ca_denfill")

    implicit none

    integer,  intent(in   ) :: adv_lo(3), adv_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: adv(adv_lo(1):adv_hi(1),adv_lo(2):adv_hi(2),adv_lo(3):adv_hi(3))

    call denfill(adv, adv_lo, adv_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_denfill


  
#ifdef GRAVITY
  subroutine phigravfill(phi, phi_lo, phi_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: phi_lo(3), phi_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: phi(phi_lo(1):phi_hi(1),phi_lo(2):phi_hi(2),phi_lo(3):phi_hi(3))

    call amrex_filccn(phi_lo, phi_hi, phi, phi_lo, phi_hi, 1, domlo, domhi, delta, xlo, bc)

  end subroutine phigravfill


  subroutine ca_phigravfill(phi,phi_lo,phi_hi,domlo,domhi,delta,xlo,time,bc) &
                            bind(C, name="ca_phigravfill")

    implicit none

    integer,  intent(in   ) :: phi_lo(3), phi_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: phi(phi_lo(1):phi_hi(1),phi_lo(2):phi_hi(2),phi_lo(3):phi_hi(3))

    call phigravfill(phi, phi_lo, phi_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_phigravfill
  

  subroutine gravxfill(grav,grav_lo,grav_hi,domlo,domhi,delta,xlo,time,bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. grav_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. grav_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(grav_lo, grav_hi, grav, grav_lo, grav_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine gravxfill


  subroutine ca_gravxfill(grav,grav_lo,grav_hi,domlo,domhi,delta,xlo,time,bc) &
                          bind(C, name="ca_gravxfill")

    implicit none

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    call gravxfill(grav, grav_lo, grav_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_gravxfill


  subroutine gravyfill(grav, grav_lo, grav_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. grav_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. grav_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(grav_lo, grav_hi, grav, grav_lo, grav_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine gravyfill


  subroutine ca_gravyfill(grav,grav_lo,grav_hi,domlo,domhi,delta,xlo,time,bc) &
                          bind(C, name="ca_gravyfill")

    implicit none

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    call gravyfill(grav, grav_lo, grav_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_gravyfill


  subroutine gravzfill(grav, grav_lo, grav_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. grav_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. grav_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(grav_lo, grav_hi, grav, grav_lo, grav_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine gravzfill


  subroutine ca_gravzfill(grav,grav_lo,grav_hi,domlo,domhi,delta,xlo,time,bc) &
                          bind(C, name="ca_gravzfill")

    implicit none

    integer,  intent(in   ) :: grav_lo(3), grav_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: grav(grav_lo(1):grav_hi(1),grav_lo(2):grav_hi(2),grav_lo(3):grav_hi(3))

    call gravzfill(grav, grav_lo, grav_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_gravzfill
#endif

  

#ifdef ROTATION
  subroutine phirotfill(phi, phi_lo, phi_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: phi_lo(3), phi_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: phi(phi_lo(1):phi_hi(1),phi_lo(2):phi_hi(2),phi_lo(3):phi_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. phi_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. phi_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(phi_lo, phi_hi, phi, phi_lo, phi_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine phirotfill


  subroutine ca_phirotfill(phi,phi_lo,phi_hi,domlo,domhi,delta,xlo,time,bc) &
                           bind(C, name="ca_phirotfill")

    implicit none

    integer,  intent(in   ) :: phi_lo(3), phi_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: phi(phi_lo(1):phi_hi(1),phi_lo(2):phi_hi(2),phi_lo(3):phi_hi(3))

    call phirotfill(phi, phi_lo, phi_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_phirotfill
  

  subroutine rotxfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. rot_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. rot_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(rot_lo, rot_hi, rot, rot_lo, rot_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine rotxfill


  subroutine ca_rotxfill(rot,rot_lo,rot_hi,domlo,domhi,delta,xlo,time,bc) &
                         bind(C, name="ca_rotxfill")

    implicit none

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    call rotxfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_rotxfill


  subroutine rotyfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. rot_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. rot_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(rot_lo, rot_hi, rot, rot_lo, rot_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine rotyfill


  subroutine ca_rotyfill(rot,rot_lo,rot_hi,domlo,domhi,delta,xlo,time,bc) &
                         bind(C, name="ca_rotyfill")

    implicit none

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    call rotyfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_rotyfill


  subroutine rotzfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. rot_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. rot_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(rot_lo, rot_hi, rot, rot_lo, rot_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine rotzfill


  subroutine ca_rotzfill(rot,rot_lo,rot_hi,domlo,domhi,delta,xlo,time,bc) &
                         bind(C, name="ca_rotzfill")

    implicit none

    integer,  intent(in   ) :: rot_lo(3), rot_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rot(rot_lo(1):rot_hi(1),rot_lo(2):rot_hi(2),rot_lo(3):rot_hi(3))

    call rotzfill(rot, rot_lo, rot_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_rotzfill
#endif  


#ifdef REACTIONS
  subroutine reactfill(react, react_lo, react_hi, domlo, domhi, delta, xlo, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: react_lo(3), react_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3)
    real(rt), intent(inout) :: react(react_lo(1):react_hi(1),react_lo(2):react_hi(2),react_lo(3):react_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. react_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. react_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(react_lo, react_hi, react, react_lo, react_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine reactfill


  subroutine ca_reactfill(react,react_lo,react_hi,domlo,domhi,delta,xlo,time,bc) &
                          bind(C, name="ca_reactfill")

    implicit none

    integer,  intent(in   ) :: react_lo(3), react_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: react(react_lo(1):react_hi(1),react_lo(2):react_hi(2),react_lo(3):react_hi(3))

    call reactfill(react, react_lo, react_hi, domlo, domhi, delta, xlo, bc)

  end subroutine ca_reactfill
#endif



#ifdef RADIATION
  subroutine radfill(rad, rad_lo, rad_hi, domlo, domhi, delta, xlo, time, bc)

    use amrex_filcc_module, only: amrex_filccn

    implicit none

    include 'AMReX_bc_types.fi'

    integer,  intent(in   ) :: rad_lo(3), rad_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rad(rad_lo(1):rad_hi(1),rad_lo(2):rad_hi(2),rad_lo(3):rad_hi(3))

    integer :: d
    integer :: bc_temp(dim,2)

    ! handle an external BC via extrapolation here 
    bc_temp(:,:) = bc(:,:)

    do d = 1, dim
       if (bc(d,1) == EXT_DIR .and. rad_lo(d) < domlo(d)) then
          bc_temp(d,1) = FOEXTRAP
       end if

       if (bc(d,2) == EXT_DIR .and. rad_hi(d) > domhi(d)) then
          bc_temp(d,2) = FOEXTRAP
       end if
    end do

    call amrex_filccn(rad_lo, rad_hi, rad, rad_lo, rad_hi, 1, domlo, domhi, delta, xlo, bc_temp)

  end subroutine radfill


  subroutine ca_radfill(rad,rad_lo,rad_hi,domlo,domhi,delta,xlo,time,bc) &
                        bind(C, name="ca_radfill")

    implicit none

    integer,  intent(in   ) :: rad_lo(3), rad_hi(3)
    integer,  intent(in   ) :: bc(dim,2)
    integer,  intent(in   ) :: domlo(3), domhi(3)
    real(rt), intent(in   ) :: delta(3), xlo(3), time
    real(rt), intent(inout) :: rad(rad_lo(1):rad_hi(1),rad_lo(2):rad_hi(2),rad_lo(3):rad_hi(3))

    call radfill(rad, rad_lo, rad_hi, domlo, domhi, delta, xlo, time, bc)

  end subroutine ca_radfill
#endif
  
end module bc_fill_module
