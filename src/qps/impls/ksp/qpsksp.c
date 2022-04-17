
#include <../src/qps/impls/ksp/qpskspimpl.h>

#undef __FUNCT__
#define __FUNCT__ "QPSKSPConverged_KSP"
static PetscErrorCode QPSKSPConverged_KSP(KSP ksp,PetscInt i,PetscReal rnorm,KSPConvergedReason *reason,void *ctx)
{
  QPS qps = (QPS) ctx;

  PetscFunctionBegin;
  qps->iteration = i;
  qps->rnorm = rnorm;
  CHKERRQ((*qps->convergencetest)(qps,reason));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "QPSKSPSynchronize_KSP"
/* synchronize operators and tolerances of the QPS and its underlying KSP */
static PetscErrorCode QPSKSPSynchronize_KSP(QPS qps)
{
  QPS_KSP          *qpsksp = (QPS_KSP*)qps->data;
  KSP              ksp = qpsksp->ksp;
  PC               pc_ksp, pc_qp;
  QP               qp;

  PetscFunctionBegin;
  CHKERRQ(QPSGetSolvedQP(qps, &qp));

  CHKERRQ(KSPGetPC(ksp, &pc_ksp));
  CHKERRQ(QPGetPC(qp, &pc_qp));
  if (pc_ksp != pc_qp) {
    CHKERRQ(KSPSetPC(ksp, pc_qp));
  }

  CHKERRQ(KSPSetOperators(ksp,qp->A,qp->A));
  CHKERRQ(KSPSetConvergenceTest(ksp, QPSKSPConverged_KSP, qps, NULL));
  CHKERRQ(KSPSetTolerances(ksp, qps->rtol, qps->atol, qps->divtol, qps->max_it));  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "QPSKSPSetKSP"
PetscErrorCode QPSKSPSetKSP(QPS qps,KSP ksp)
{
  PetscBool flg;
  QPS_KSP *qpsksp;
  const char *prefix;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qps,QPS_CLASSID,1);
  PetscValidHeaderSpecific(ksp,KSP_CLASSID,2);
  CHKERRQ(PetscObjectTypeCompare((PetscObject)qps,QPSKSP,&flg));
  if (!flg) SETERRQ(((PetscObject)qps)->comm,PETSC_ERR_SUP,"This is a QPSKSP specific routine!");
  qpsksp = (QPS_KSP*)qps->data;
  
  CHKERRQ(KSPDestroy(&qpsksp->ksp));
  qpsksp->ksp = ksp;
  CHKERRQ(PetscObjectReference((PetscObject)ksp));
  
  CHKERRQ(QPSGetOptionsPrefix(qps,&prefix));
  CHKERRQ(KSPSetOptionsPrefix(ksp,prefix)); 
  CHKERRQ(KSPAppendOptionsPrefix(ksp,"qps_"));  
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPSKSPGetKSP"
PetscErrorCode QPSKSPGetKSP(QPS qps,KSP *ksp)
{
  PetscBool flg;
  QPS_KSP *qpsksp;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qps,QPS_CLASSID,1);
  PetscValidPointer(ksp,2);
  CHKERRQ(PetscObjectTypeCompare((PetscObject)qps,QPSKSP,&flg));
  if (!flg) SETERRQ(((PetscObject)qps)->comm,PETSC_ERR_SUP,"This is a QPSKSP specific routine!");
  qpsksp = (QPS_KSP*)qps->data;
  *ksp = qpsksp->ksp;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPSKSPSetType"
PetscErrorCode QPSKSPSetType(QPS qps,KSPType type)
{
  PetscBool flg;
  QPS_KSP *qpsksp;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qps,QPS_CLASSID,1);
  CHKERRQ(PetscObjectTypeCompare((PetscObject)qps,QPSKSP,&flg));
  if (!flg) SETERRQ(((PetscObject)qps)->comm,PETSC_ERR_SUP,"This is a QPSKSP specific routine!");
  qpsksp = (QPS_KSP*)qps->data;
  CHKERRQ(KSPSetType(qpsksp->ksp,type));
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "QPSKSPGetType"
PetscErrorCode QPSKSPGetType(QPS qps,KSPType *type)
{
  PetscBool flg;
  QPS_KSP *qpsksp;
  
  PetscFunctionBegin;
  PetscValidHeaderSpecific(qps,QPS_CLASSID,1);
  CHKERRQ(PetscObjectTypeCompare((PetscObject)qps,QPSKSP,&flg));
  if (!flg) SETERRQ(((PetscObject)qps)->comm,PETSC_ERR_SUP,"This is a QPSKSP specific routine!");
  qpsksp = (QPS_KSP*)qps->data;
  CHKERRQ(KSPGetType(qpsksp->ksp,type));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "QPSSetUp_KSP"
PetscErrorCode QPSSetUp_KSP(QPS qps)
{
  QPS_KSP          *qpsksp = (QPS_KSP*)qps->data;
  KSP              ksp = qpsksp->ksp;
  
  PetscFunctionBegin;
  CHKERRQ(QPSKSPSynchronize_KSP(qps));
  if (qpsksp->setfromoptionscalled) CHKERRQ(KSPSetFromOptions(ksp));
  CHKERRQ(KSPSetUp(ksp));
  PetscFunctionReturn(0);  
}

#undef __FUNCT__  
#define __FUNCT__ "QPSSolve_KSP"
PetscErrorCode QPSSolve_KSP(QPS qps)
{
  QPS_KSP          *qpsksp = (QPS_KSP*)qps->data;
  KSP              ksp = qpsksp->ksp;
  Vec              b,x;
  QP               qp;
  
  PetscFunctionBegin;
  CHKERRQ(QPSGetSolvedQP(qps,&qp));
  CHKERRQ(QPSKSPSynchronize_KSP(qps));
  CHKERRQ(QPGetRhs(qp,&b));
  CHKERRQ(QPGetSolutionVector(qp,&x));
  CHKERRQ(KSPSolve(ksp, b, x));
  CHKERRQ(KSPGetIterationNumber(ksp,&qps->iteration));
  CHKERRQ(KSPGetResidualNorm(   ksp,&qps->rnorm));
  CHKERRQ(KSPGetConvergedReason(ksp,&qps->reason));
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "QPSSetFromOptions_KSP"
PetscErrorCode QPSSetFromOptions_KSP(PetscOptionItems *PetscOptionsObject,QPS qps)
{
  QPS_KSP          *qpsksp = (QPS_KSP*)qps->data;
  
  PetscFunctionBegin;
  qpsksp->setfromoptionscalled = PETSC_TRUE;
  PetscFunctionReturn(0);  
}

#undef __FUNCT__  
#define __FUNCT__ "QPSView_KSP"
PetscErrorCode QPSView_KSP(QPS qps, PetscViewer v)
{
  QPS_KSP          *qpsksp = (QPS_KSP*)qps->data;
  KSP              ksp = qpsksp->ksp;
  
  PetscFunctionBegin;
  CHKERRQ(KSPView(ksp, v));
  PetscFunctionReturn(0);  
}

#undef __FUNCT__
#define __FUNCT__ "QPSViewConvergence_KSP"
PetscErrorCode QPSViewConvergence_KSP(QPS qps, PetscViewer v)
{
  PetscBool     iascii;

  PetscFunctionBegin;
  CHKERRQ(PetscObjectTypeCompare((PetscObject)v,PETSCVIEWERASCII,&iascii));
  if (iascii) {
    KSPType ksptype;
    CHKERRQ(QPSKSPGetType(qps, &ksptype));
    CHKERRQ(PetscViewerASCIIPrintf(v, "KSPType: %s\n", ksptype));
  }
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "QPSDestroy_KSP"
PetscErrorCode QPSDestroy_KSP(QPS qps)
{
  QPS_KSP         *qpsksp = (QPS_KSP*)qps->data;

  PetscFunctionBegin;
  CHKERRQ(KSPDestroy(&qpsksp->ksp));
  CHKERRQ(QPSDestroyDefault(qps));
  PetscFunctionReturn(0);  
}

#undef __FUNCT__  
#define __FUNCT__ "QPSIsQPCompatible_KSP"
PetscErrorCode QPSIsQPCompatible_KSP(QPS qps,QP qp,PetscBool *flg)
{
  Mat Beq,Bineq;
  Vec ceq,cineq;
  QPC qpc;
  
  PetscFunctionBegin;
  *flg = PETSC_TRUE;
  CHKERRQ(QPGetEq(qp,&Beq,&ceq));
  CHKERRQ(QPGetIneq(qp,&Bineq,&cineq));
  CHKERRQ(QPGetQPC(qp,&qpc));
  if (Beq || ceq || Bineq || cineq || qpc)
  {
    *flg = PETSC_FALSE;
  }
  PetscFunctionReturn(0);   
}

#undef __FUNCT__  
#define __FUNCT__ "QPSCreate_KSP"
FLLOP_EXTERN PetscErrorCode QPSCreate_KSP(QPS qps)
{
  QPS_KSP         *qpsksp;
  MPI_Comm        comm;
  
  PetscFunctionBegin;
  CHKERRQ(PetscObjectGetComm((PetscObject)qps,&comm));
  CHKERRQ(PetscNewLog(qps,&qpsksp));
  qps->data                  = (void*)qpsksp;
  qpsksp->setfromoptionscalled = PETSC_FALSE;
  
  /*
       Sets the functions that are associated with this data structure 
       (in C++ this is the same as defining virtual functions)
  */
  qps->ops->setup            = QPSSetUp_KSP;
  qps->ops->solve            = QPSSolve_KSP;
  qps->ops->destroy          = QPSDestroy_KSP;
  qps->ops->isqpcompatible   = QPSIsQPCompatible_KSP;
  qps->ops->setfromoptions   = QPSSetFromOptions_KSP;
  qps->ops->view             = QPSView_KSP;
  qps->ops->viewconvergence  = QPSViewConvergence_KSP;
  
  CHKERRQ(KSPCreate(comm,&qpsksp->ksp));
  CHKERRQ(KSPSetOptionsPrefix(qpsksp->ksp,"qps_"));
  CHKERRQ(PetscLogObjectParent((PetscObject)qps,(PetscObject)qpsksp->ksp));
  CHKERRQ(PetscObjectIncrementTabLevel((PetscObject)qpsksp->ksp,(PetscObject)qps,1));
  CHKERRQ(KSPSetType(qpsksp->ksp,KSPCG));
  CHKERRQ(KSPSetNormType(qpsksp->ksp,KSP_NORM_UNPRECONDITIONED));
  CHKERRQ(KSPSetInitialGuessNonzero(qpsksp->ksp, PETSC_TRUE));
  {
    PC pc;
    CHKERRQ(KSPGetPC(qpsksp->ksp, &pc));
    CHKERRQ(PCSetType(pc, PCNONE));
  }
  PetscFunctionReturn(0);
}
