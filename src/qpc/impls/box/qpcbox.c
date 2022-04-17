#include <../src/qpc/impls/box/qpcboximpl.h>

#undef __FUNCT__
#define __FUNCT__ "QPCSetUp_Box"
PetscErrorCode QPCSetUp_Box(QPC qpc)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;
  Vec                   lb;

  PetscFunctionBegin;

  /* prepare lambdawork vector based on the layout of lb */
  lb = ctx->lb;
  CHKERRQ(VecDuplicate(lb,&(qpc->lambdawork)));

  // TODO: verify layout of ub somewhere in setup or in create function

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCGrads_Box"
static PetscErrorCode QPCGrads_Box(QPC qpc, Vec x, Vec g, Vec gf, Vec gc)
{
  Vec                   lb,ub;
  QPC_Box               *ctx = (QPC_Box*)qpc->data;

  PetscScalar           *x_a, *lb_a, *ub_a, *g_a, *gf_a, *gc_a;
  PetscInt              n_local, i;

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;
  CHKERRQ(VecGetLocalSize(x,&n_local));
  CHKERRQ(VecGetArray(x, &x_a));
  if (lb) CHKERRQ(VecGetArray(lb, &lb_a));
  if (ub) CHKERRQ(VecGetArray(ub, &ub_a));
  CHKERRQ(VecGetArray(g, &g_a));
  CHKERRQ(VecGetArray(gf, &gf_a));
  CHKERRQ(VecGetArray(gc, &gc_a));

  /* TODO create free/active IS? */
  for (i = 0; i < n_local; i++){
    if (lb && PetscAbsScalar(x_a[i] - lb_a[i]) <= qpc->astol) {
      /* active lower bound */
      gf_a[i] = 0.0;
      gc_a[i]= PetscMin(g_a[i],0.0);
    //} else if (ub && x_a[i] >= ub_a[i]) {
    } else if (ub && PetscAbsScalar(x_a[i] -  ub_a[i]) <= qpc->astol) {
      /* active upper bound */
      gf_a[i] = 0.0;
      gc_a[i]= PetscMax(g_a[i],0.0);
    } else {
      /* index of this component is in FREE SET */
      gf_a[i] = g_a[i];
    }
  }

  CHKERRQ(VecRestoreArray(x, &x_a));
  if (lb) CHKERRQ(VecRestoreArray(lb, &lb_a));
  if (ub) CHKERRQ(VecRestoreArray(ub, &ub_a));
  CHKERRQ(VecRestoreArray(g, &g_a));
  CHKERRQ(VecRestoreArray(gf, &gf_a));
  CHKERRQ(VecRestoreArray(gc, &gc_a));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCGradReduced_Box"
static PetscErrorCode QPCGradReduced_Box(QPC qpc, Vec x, Vec gf, PetscReal alpha, Vec gr)
{
  Vec                   lb,ub;
  QPC_Box               *ctx = (QPC_Box*)qpc->data;

  PetscScalar           *x_a, *lb_a, *ub_a, *gf_a, *gr_a;
  PetscInt              n_local, i;

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;
  CHKERRQ(VecGetLocalSize(x,&n_local));
  CHKERRQ(VecGetArray(x, &x_a));
  if (lb) CHKERRQ(VecGetArray(lb, &lb_a));
  if (ub) CHKERRQ(VecGetArray(ub, &ub_a));
  CHKERRQ(VecGetArray(gf, &gf_a));
  CHKERRQ(VecGetArray(gr, &gr_a));

  for (i = 0; i < n_local; i++){
    if (lb && gf_a[i] > 0.0) {
      gr_a[i] = PetscMin(gf_a[i],(x_a[i]-lb_a[i])/alpha);
    } else if (ub && gf_a[i] < 0.0) {
      gr_a[i] = PetscMax(gf_a[i],(x_a[i]-ub_a[i])/alpha);
    }
  }

  CHKERRQ(VecRestoreArray(x, &x_a));
  if (lb) CHKERRQ(VecRestoreArray(lb, &lb_a));
  if (ub) CHKERRQ(VecRestoreArray(ub, &ub_a));
  CHKERRQ(VecRestoreArray(gf, &gf_a));
  CHKERRQ(VecRestoreArray(gr, &gr_a));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCFeas_Box"
static PetscErrorCode QPCFeas_Box(QPC qpc,Vec x, Vec d, PetscScalar *alpha)
{
  Vec                   lb,ub;
  QPC_Box               *ctx = (QPC_Box*)qpc->data;
  PetscScalar           alpha_temp, alpha_i;

  PetscScalar           *x_a, *d_a, *lb_a, *ub_a;
  PetscInt              n_local, i;

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;

  alpha_temp = PETSC_INFINITY;

  CHKERRQ(VecGetLocalSize(x,&n_local));
  CHKERRQ(VecGetArray(x,&x_a));
  CHKERRQ(VecGetArray(d,&d_a));
  if (lb) CHKERRQ(VecGetArray(lb,&lb_a));
  if (ub) CHKERRQ(VecGetArray(ub,&ub_a));

  for(i=0;i < n_local;i++){
    if(d_a[i] > 0 && lb && lb_a[i] > PETSC_NINFINITY) {
      alpha_i = x_a[i]-lb_a[i];
      alpha_i = alpha_i/d_a[i];
      if (alpha_i < alpha_temp){
          alpha_temp = alpha_i;
      }
    }

    if(d_a[i] < 0 && ub && ub_a[i] < PETSC_INFINITY) {
      alpha_i = (x_a[i]-ub_a[i]);
      alpha_i = alpha_i/d_a[i];

      if (alpha_i < alpha_temp){
          alpha_temp = alpha_i;
      }
    }
  }
  *alpha = alpha_temp;

  CHKERRQ(VecRestoreArray(x,&x_a));
  CHKERRQ(VecRestoreArray(d,&d_a));
  if (lb) CHKERRQ(VecRestoreArray(lb,&lb_a));
  if (ub) CHKERRQ(VecRestoreArray(ub,&ub_a));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCBoxSet_Box"
static PetscErrorCode QPCBoxSet_Box(QPC qpc,Vec lb, Vec ub)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;

  PetscFunctionBegin;
  CHKERRQ(VecDestroy(&ctx->lb));
  CHKERRQ(VecDestroy(&ctx->ub));
  CHKERRQ(VecDestroy(&ctx->llb));
  CHKERRQ(VecDestroy(&ctx->lub));
  ctx->lb = lb;
  ctx->ub = ub;
  if (lb) {
    CHKERRQ(VecDuplicate(lb,&ctx->llb));
    CHKERRQ(VecInvalidate(ctx->llb));
  }
  if (ub) {
    CHKERRQ(VecDuplicate(ub,&ctx->lub));
    CHKERRQ(VecInvalidate(ctx->lub));
  }
  CHKERRQ(PetscObjectReference((PetscObject)lb));
  CHKERRQ(PetscObjectReference((PetscObject)ub));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCBoxGet_Box"
static PetscErrorCode QPCBoxGet_Box(QPC qpc,Vec *lb,Vec *ub)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;

  PetscFunctionBegin;
  if (lb) *lb = ctx->lb;
  if (ub) *ub = ctx->ub;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCBoxGetMultipliers_Box"
static PetscErrorCode QPCBoxGetMultipliers_Box(QPC qpc,Vec *llb,Vec *lub)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;

  PetscFunctionBegin;
  if (llb) *llb = ctx->llb;
  if (lub) *lub = ctx->lub;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCIsLinear_Box"
PetscErrorCode QPCIsLinear_Box(QPC qpc,PetscBool *linear)
{
  PetscFunctionBegin;
  *linear = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCIsSubsymmetric_Box"
PetscErrorCode QPCIsSubsymmetric_Box(QPC qpc,PetscBool *subsymmetric)
{
  PetscFunctionBegin;
  *subsymmetric = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCGetBlockSize_Box"
PetscErrorCode QPCGetBlockSize_Box(QPC qpc,PetscInt *bs)
{
  PetscFunctionBegin;
  *bs = 1;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCGetNumberOfConstraints_Box"
PetscErrorCode QPCGetNumberOfConstraints_Box(QPC qpc, PetscInt *num)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;
  PetscInt          size,bs;

  PetscFunctionBegin;

  if(qpc->is){
        /* IS is present, return the size of IS */
        CHKERRQ(ISGetSize(qpc->is,&size));
        CHKERRQ(QPCGetBlockSize(qpc,&bs));
        *num = size/bs;
  } else {
        /* IS is not present, all components are constrained */
        CHKERRQ(VecGetSize(ctx->lb,&size)); /* = size of ub */
        *num = size;
  }

  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCGetConstraintFunction_Box"
PetscErrorCode QPCGetConstraintFunction_Box(QPC qpc, Vec x_sub, Vec *hx_out)
{
  QPC_Box               *ctx = (QPC_Box*)qpc->data;
  Vec                   lb,ub;
  PetscScalar           *lb_a, *ub_a, *x_a, *hx_a; /* local arrays */
  PetscInt              m; /* local size */
  PetscInt              i; /* iterator */

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;
  CHKERRQ(VecGetLocalSize(qpc->lambdawork,&m));
  CHKERRQ(VecGetArray(x_sub,&x_a));
  CHKERRQ(VecGetArray(qpc->lambdawork,&hx_a));
  if (lb) CHKERRQ(VecGetArray(lb,&lb_a));
  if (ub) CHKERRQ(VecGetArray(ub,&ub_a));

  for(i = 0; i< m; i++){
      // TODO: verify the access to arrays, with correct layout, it should be fine..
      if (lb) {
        if (ub) {
          hx_a[i] = PetscAbsScalar(x_a[i] - (ub_a[i] + lb_a[i])/2) - (ub_a[i] - lb_a[i])/2;
        } else {
          hx_a[i] = lb_a[i] - x_a[i];
        }
      } else {
        hx_a[i] = x_a[i] - ub_a[i];
      }
  }

  /* restore local arrays */
  CHKERRQ(VecRestoreArray(x_sub, &x_a));
  CHKERRQ(VecRestoreArray(qpc->lambdawork,&hx_a));
  if (lb) CHKERRQ(VecRestoreArray(lb,&lb_a));
  if (ub) CHKERRQ(VecRestoreArray(ub,&ub_a));

  *hx_out = qpc->lambdawork;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCProject_Box"
PetscErrorCode QPCProject_Box(QPC qpc, Vec x, Vec Px)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;
  Vec             lb,ub;

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;
  if (lb) {
    CHKERRQ(VecPointwiseMax(Px,x,lb));
    if (ub) CHKERRQ(VecPointwiseMin(Px,Px,ub));
  } else {
    CHKERRQ(VecPointwiseMin(Px,x,ub));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCView_Box"
PetscErrorCode QPCView_Box(QPC qpc, PetscViewer viewer)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;

  PetscFunctionBegin;
  /* print lb */
  if (ctx->lb) {
    CHKERRQ(PetscViewerASCIIPrintf(viewer, "lb:\n"));
    CHKERRQ(PetscViewerASCIIPushTab(viewer));
    CHKERRQ(VecView(ctx->lb,viewer));
    CHKERRQ(PetscViewerASCIIPopTab(viewer));
  }
  /* print ub */
  if (ctx->ub) {
    CHKERRQ(PetscViewerASCIIPrintf(viewer, "ub:\n"));
    CHKERRQ(PetscViewerASCIIPushTab(viewer));
    CHKERRQ(VecView(ctx->ub,viewer));
    CHKERRQ(PetscViewerASCIIPopTab(viewer));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCViewKKT_Box"
PetscErrorCode QPCViewKKT_Box(QPC qpc, Vec x, PetscReal normb, PetscViewer v)
{
  QPC_Box         *ctx = (QPC_Box*)qpc->data;
  Vec             lb,ub,llb,lub,r,o;
  PetscScalar     norm,dot;

  PetscFunctionBegin;
  lb = ctx->lb;
  ub = ctx->ub;
  llb = ctx->llb;
  lub = ctx->lub;
  if (lb) {
    CHKERRQ(VecDuplicate(x,&o));
    CHKERRQ(VecDuplicate(x,&r));

    /* rI = norm(min(x-lb,0)) */
    CHKERRQ(VecSet(o,0.0));                                   /* o = zeros(size(r)) */
    CHKERRQ(VecWAXPY(r, -1.0, lb, x));                        /* r = x - lb       */
    CHKERRQ(VecPointwiseMin(r,r,o));                          /* r = min(r,o)     */
    CHKERRQ(VecNorm(r,NORM_2,&norm));                         /* norm = norm(r)     */
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = ||min(x-lb,0)||      = %.2e    r/||b|| = %.2e\n",norm,norm/normb));

    /* lambda >= o  =>  examine min(lambda,o) */
    CHKERRQ(VecSet(o,0.0));                                   /* o = zeros(size(r)) */
    CHKERRQ(VecPointwiseMin(r,llb,o));
    CHKERRQ(VecNorm(r,NORM_2,&norm));                         /* norm = ||min(lambda,o)|| */
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = ||min(lambda_lb,0)|| = %.2e    r/||b|| = %.2e\n",norm,norm/normb));

    /* lambda'*(lb-x) = 0 */
    CHKERRQ(VecCopy(lb,r));
    CHKERRQ(VecAXPY(r,-1.0,x));
    {
      PetscInt i,n;
      PetscScalar *rarr;
      const PetscScalar *larr;
      CHKERRQ(VecGetLocalSize(r,&n));
      CHKERRQ(VecGetArray(r,&rarr));
      CHKERRQ(VecGetArrayRead(lb,&larr));
      for (i=0; i<n; i++) if (larr[i]<=PETSC_NINFINITY) rarr[i]=-1.0;
      CHKERRQ(VecRestoreArray(r,&rarr));
      CHKERRQ(VecRestoreArrayRead(lb,&larr));
    }
    CHKERRQ(VecDot(llb,r,&dot));
    dot = PetscAbs(dot);
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = |lambda_lb'*(lb-x)|  = %.2e    r/||b|| = %.2e\n",dot,dot/normb));

    CHKERRQ(VecDestroy(&o));
    CHKERRQ(VecDestroy(&r));
  }

  if (ub) {
    CHKERRQ(VecDuplicate(x,&o));
    CHKERRQ(VecDuplicate(x,&r));

    /* rI = norm(max(x-ub,0)) */
    CHKERRQ(VecDuplicate(x,&r));
    CHKERRQ(VecDuplicate(x,&o));
    CHKERRQ(VecSet(o,0.0));                                   /* o = zeros(size(r)) */
    CHKERRQ(VecWAXPY(r, -1.0, ub, x));                        /* r = x - ub       */
    CHKERRQ(VecPointwiseMax(r,r,o));                          /* r = max(r,o)     */
    CHKERRQ(VecNorm(r,NORM_2,&norm));                         /* norm = norm(r)     */
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = ||max(x-ub,0)||      = %.2e    r/||b|| = %.2e\n",norm,norm/normb));

    /* lambda >= o  =>  examine min(lambda,o) */
    CHKERRQ(VecSet(o,0.0));                                   /* o = zeros(size(r)) */
    CHKERRQ(VecPointwiseMin(r,lub,o));
    CHKERRQ(VecNorm(r,NORM_2,&norm));                         /* norm = ||min(lambda,o)|| */
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = ||min(lambda_ub,0)|| = %.2e    r/||b|| = %.2e\n",norm,norm/normb));

    /* lambda'*(x-ub) = 0 */
    CHKERRQ(VecCopy(ub,r));
    CHKERRQ(VecAYPX(r,-1.0,x));
    {
      PetscInt i,n;
      PetscScalar *rarr;
      const PetscScalar *uarr;
      CHKERRQ(VecGetLocalSize(r,&n));
      CHKERRQ(VecGetArray(r,&rarr));
      CHKERRQ(VecGetArrayRead(ub,&uarr));
      for (i=0; i<n; i++) if (uarr[i]>=PETSC_INFINITY) rarr[i]=1.0;
      CHKERRQ(VecRestoreArray(r,&rarr));
      CHKERRQ(VecRestoreArrayRead(ub,&uarr));
    }
    CHKERRQ(VecDot(lub,r,&dot));
    dot = PetscAbs(dot);
    CHKERRQ(PetscViewerASCIIPrintf(v,"r = |lambda_ub'*(x-ub)|  = %.2e    r/||b|| = %.2e\n",dot,dot/normb));

    CHKERRQ(VecDestroy(&o));
    CHKERRQ(VecDestroy(&r));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCDestroy_Box"
PetscErrorCode QPCDestroy_Box(QPC qpc)
{
  QPC_Box      *ctx = (QPC_Box*)qpc->data;

  PetscFunctionBegin;
  if (ctx->lb) {
    CHKERRQ(VecDestroy(&ctx->lb));
    CHKERRQ(VecDestroy(&ctx->llb));
  }
  if (ctx->ub) {
    CHKERRQ(VecDestroy(&ctx->ub));
    CHKERRQ(VecDestroy(&ctx->lub));
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCCreate_Box"
FLLOP_EXTERN PetscErrorCode QPCCreate_Box(QPC qpc)
{
  QPC_Box      *ctx;

  PetscFunctionBegin;
  CHKERRQ(PetscNewLog(qpc,&ctx));
  qpc->data = (void*)ctx;

  /* set general QPC functions already implemented for this QPC type */
  qpc->ops->destroy                     = QPCDestroy_Box;
  qpc->ops->setup                       = QPCSetUp_Box;
  qpc->ops->view                        = QPCView_Box;
  qpc->ops->viewkkt                     = QPCViewKKT_Box;
  qpc->ops->getblocksize                = QPCGetBlockSize_Box;
  qpc->ops->getconstraintfunction       = QPCGetConstraintFunction_Box;
  qpc->ops->getnumberofconstraints      = QPCGetNumberOfConstraints_Box;
  qpc->ops->project                     = QPCProject_Box;
  qpc->ops->islinear                    = QPCIsLinear_Box;
  qpc->ops->issubsymmetric              = QPCIsSubsymmetric_Box;
  qpc->ops->feas                        = QPCFeas_Box;
  qpc->ops->grads                       = QPCGrads_Box;
  qpc->ops->gradreduced                 = QPCGradReduced_Box;

  /* set type-specific functions */
  CHKERRQ(PetscObjectComposeFunction((PetscObject)qpc,"QPCBoxSet_Box_C",QPCBoxSet_Box));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)qpc,"QPCBoxGet_Box_C",QPCBoxGet_Box));
  CHKERRQ(PetscObjectComposeFunction((PetscObject)qpc,"QPCBoxGetMultipliers_Box_C",QPCBoxGetMultipliers_Box));

  /* initialize type-specific inner data */
  ctx->lb                         = NULL;
  ctx->ub                         = NULL;
  ctx->llb                        = NULL;
  ctx->lub                        = NULL;
  PetscFunctionReturn(0);
}


#undef __FUNCT__
#define __FUNCT__ "QPCBoxSet"
PetscErrorCode QPCBoxSet(QPC qpc,Vec lb, Vec ub)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qpc,QPC_CLASSID,1);
  if (lb) PetscValidHeaderSpecific(lb,VEC_CLASSID,2);
  if (ub) PetscValidHeaderSpecific(ub,VEC_CLASSID,3);
  if (!lb && !ub) SETERRQ(PetscObjectComm((PetscObject)qpc),PETSC_ERR_ARG_NULL,"lb and ub cannot be both NULL");
#if defined(PETSC_USE_DEBUG)
  if (lb && ub) {
    Vec diff;
    PetscReal min;

    CHKERRQ(VecDuplicate(lb,&diff));
    CHKERRQ(VecWAXPY(diff,-1.0,lb,ub));
    CHKERRQ(VecMin(diff,NULL,&min));
    /* TODO verify that algorithms work with min = 0 */
    if (min < 0.0) SETERRQ(PetscObjectComm((PetscObject)qpc),PETSC_ERR_ARG_INCOMP,"lb components must be smaller than ub components");
    CHKERRQ(VecDestroy(&diff));
  }
#endif

  PetscUseMethod(qpc,"QPCBoxSet_Box_C",(QPC,Vec,Vec),(qpc,lb,ub));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCBoxGet"
PetscErrorCode QPCBoxGet(QPC qpc,Vec *lb, Vec *ub)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qpc,QPC_CLASSID,1);
  if (lb) PetscValidPointer(lb,2);
  if (ub) PetscValidPointer(ub,3);

  PetscUseMethod(qpc,"QPCBoxGet_Box_C",(QPC,Vec*,Vec*),(qpc,lb,ub));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCBoxGetMultipliers"
PetscErrorCode QPCBoxGetMultipliers(QPC qpc,Vec *llb,Vec *lub)
{
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qpc,QPC_CLASSID,1);
  if (llb) PetscValidPointer(llb,2);
  if (lub) PetscValidPointer(lub,3);

  PetscUseMethod(qpc,"QPCBoxGetMultipliers_Box_C",(QPC,Vec*,Vec*),(qpc,llb,lub));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPCCreateBox"
/*@
QPCCreateBox - create QPC Box instance; set the type of QPC to Box, set vector of variables, set index set of constrained components, set the value of lower and upper bounds

Parameters:
+ comm - MPI comm
. x - vector of variables
. is - index set of constrained variables, if NULL then all compoments are constrained
. lb - the vector with lower bounds values
. ub - the vector with upper bounds values
- qpc_out - pointer to QPC
@*/
PetscErrorCode QPCCreateBox(MPI_Comm comm,IS is,Vec lb,Vec ub,QPC *qpc_out)
{
  QPC qpc;

  PetscFunctionBegin;

  /* verify input data */
  if (is) PetscValidHeaderSpecific(is,IS_CLASSID,2);
  if (lb) PetscValidHeaderSpecific(lb,VEC_CLASSID,3);
  if (ub) PetscValidHeaderSpecific(ub,VEC_CLASSID,4);
  PetscValidPointer(qpc_out,5);

  CHKERRQ(QPCCreate(comm,&qpc));
  CHKERRQ(QPCSetIS(qpc,is));
  CHKERRQ(QPCSetType(qpc,QPCBOX));
  CHKERRQ(QPCBoxSet(qpc,lb,ub));
  *qpc_out = qpc;
  PetscFunctionReturn(0);
}
