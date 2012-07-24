
/**************************************************************************
 * This file is part of Celera Assembler, a software program that
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, Applera Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received (LICENSE.txt) a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/

static char *rcsid = "$Id: Input_CGW.c,v 1.76 2012-07-24 12:06:46 brianwalenz Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "AS_global.h"
#include "AS_UTL_Var.h"
#include "AS_CGW_dataTypes.h"
#include "AS_PER_gkpStore.h"
#include "ScaffoldGraph_CGW.h"
#include "Globals_CGW.h"
#include "ScaffoldGraph_CGW.h"
#include "Output_CGW.h"
#include "Input_CGW.h"


void ProcessInputUnitig(MultiAlignT *uma);


int ProcessInput(int optind, int argc, char *argv[]){
  int i,j = 0;
  int32  numFRG = 0;
  int32  numUTG = 0;


  fprintf(stderr, "Reading fragments.\n");

  EnableRange_VA(ScaffoldGraph->CIFrags, ScaffoldGraph->gkpStore->gkStore_getNumFragments() + 1);

  gkFragment  fr;
  gkStream   *fs = new gkStream(ScaffoldGraph->gkpStore, 0, 0, GKFRAGMENT_INF);

  //  There is no zero fragment.  Make the data junk (this also sets isDeleted, so if we do ever
  //  happen to use the fragment, it'll generally be skipped.

  memset(GetCIFragT(ScaffoldGraph->CIFrags, 0), 0xff, sizeof(CIFragT));

  while (fs->next(&fr)) {
    CIFragT   *cifrag = GetCIFragT(ScaffoldGraph->CIFrags, fr.gkFragment_getReadIID());

    cifrag->read_iid                              = fr.gkFragment_getReadIID();
    cifrag->mate_iid                              = fr.gkFragment_getMateIID();
    cifrag->dist                                  = fr.gkFragment_getLibraryIID();

    cifrag->cid                                   = NULLINDEX;
    cifrag->CIid                                  = NULLINDEX;
    cifrag->contigID                              = NULLINDEX;

    cifrag->offset5p.mean                         = 0.0;
    cifrag->offset5p.variance                     = 0.0;
    cifrag->offset3p.mean                         = 0.0;
    cifrag->offset3p.variance                     = 0.0;

    cifrag->contigOffset5p.mean                   = 0.0;
    cifrag->contigOffset5p.variance               = 0.0;
    cifrag->contigOffset3p.mean                   = 0.0;
    cifrag->contigOffset3p.variance               = 0.0;

    cifrag->flags.bits.hasInternalOnlyCILinks     = FALSE;
    cifrag->flags.bits.hasInternalOnlyContigLinks = FALSE;
    cifrag->flags.bits.isPlaced                   = FALSE;
    cifrag->flags.bits.isSingleton                = FALSE;
    cifrag->flags.bits.isChaff                    = FALSE;
    cifrag->flags.bits.isDeleted                  = fr.gkFragment_getIsDeleted();
    cifrag->flags.bits.innieMate                  = (fr.gkFragment_getOrientation() == AS_READ_ORIENT_INNIE);
    cifrag->flags.bits.hasMate                    = (fr.gkFragment_getMateIID() > 0);

    cifrag->flags.bits.edgeStatus                 = (fr.gkFragment_getMateIID() > 0) ? UNKNOWN_EDGE_STATUS : INVALID_EDGE_STATUS;
    cifrag->flags.bits.chunkLabel                 = AS_SINGLETON;

    cifrag->flags.bits.mateDetail                 = UNASSIGNED_MATE;

    if ((++numFRG % 1000000) == 0) {
      fprintf(stderr, "...processed "F_S32" fragments.\n", numFRG);
    }
  }

  delete fs;

  fprintf(stderr, "Reading unitigs.\n");

  //  We flush the cache while loading, as each unitig is immediately copied to a contig.  Even
  //  thought the immediate next step (of checking for chimeric unitigs) will reload all unitigs
  //  again, the flush is useful (it gets rid of that second copy in a unitig).  During assembly, we
  //  usually never need ALL this stuff loaded at the same time.

  for (int32 i=0; i<ScaffoldGraph->tigStore->numUnitigs(); i++) {
    MultiAlignT   *uma = ScaffoldGraph->tigStore->loadMultiAlign(i, TRUE);

    if (uma == NULL) {
      ChunkInstanceT  CI;

      fprintf(stderr, "WARNING:  Unitig %d does not exist.\n", i);

      memset(&CI, 0, sizeof(ChunkInstanceT));

      //  BEWARE!  Magic.  We try to fake the process of ProcessInputUnitig() then DeleteGraphNode().

      CI.id                                  = i;                    //  uma->maID, if only it existed

      CI.bpLength.mean                       = 0;          //  Boilerplate from below, to NULLINDEX
      CI.bpLength.variance                   = 1.0;
      CI.edgeHead                            = NULLINDEX;
      CI.setID                               = NULLINDEX;
      CI.scaffoldID                          = NULLINDEX;
      CI.indexInScaffold                     = NULLINDEX;
      CI.prevScaffoldID                      = NULLINDEX;
      CI.essentialEdgeA                      = NULLINDEX;
      CI.essentialEdgeB                      = NULLINDEX;
      CI.smoothExpectedCID                   = NULLINDEX;
      CI.BEndNext                            = NULLINDEX;
      CI.AEndNext                            = NULLINDEX;

      CI.info.CI.contigID                    = NULLINDEX;
      CI.info.CI.source                      = NULLINDEX;

      CI.flags.bits.isCI                     = TRUE;  //  Crashes NextGraphNodeIterator() if not set.
      CI.flags.bits.isDead                   = TRUE;  //  DOA.

      //  Mark this as a unique CI, to prevent it from being used in stones.  Stones crashes on
      //  these empty unitigs.

      CI.flags.bits.isUnique                 = 1;
      CI.type                                = DISCRIMINATORUNIQUECHUNK_CGW;

      //  And while we're at it, just mark it chaff too.

      CI.flags.bits.isChaff                  = TRUE;

      //  Add it to the graph.

      SetChunkInstanceT(ScaffoldGraph->CIGraph->nodes, CI.id, &CI);

      continue;
    }

    assert(i == uma->maID);

    if (1 != GetNumIntUnitigPoss(uma->u_list))
      fprintf(stderr, "ERROR:  Unitig %d has no placement; probably not run through consensus.\n", i);
    assert(1 == GetNumIntUnitigPoss(uma->u_list));

    if (i != GetIntUnitigPos(uma->u_list, 0)->ident)
      fprintf(stderr, "ERROR:  Unitig %d has incorrect unitig ident; error in utgcns or utgcnsfix or manual fixes to unitig.\n", i);
    assert(i == GetIntUnitigPos(uma->u_list, 0)->ident);

    MultiAlignT   *cma = CopyMultiAlignT(NULL, uma);

    ScaffoldGraph->tigStore->insertMultiAlign(cma, FALSE, TRUE);

    ProcessInputUnitig(uma);

    if ((++numUTG % 100000) == 0) {
      fprintf(stderr, "...processed "F_S32" unitigs.\n", numUTG);
      ScaffoldGraph->tigStore->flushCache();
    }
  }

  ScaffoldGraph->tigStore->flushCache();

  fprintf(stderr, "Checking sanity of loaded fragments.\n");
 
  uint32  numErrors = 0;

  for (int32 i=1, s=GetNumCIFragTs(ScaffoldGraph->CIFrags); i<s; i++) {
    CIFragT     *cifrag = GetCIFragT(ScaffoldGraph->CIFrags, i);
 
    if (cifrag->flags.bits.isDeleted)
      continue;

    //  We could instead delete these fragments from the assembly.  That is somewhat difficult to do
    //  here, since we (a) don't have the gkpStore opened for writing and (b) don't have permission to
    //  do that anyway.

    if ((cifrag->cid == NULLINDEX) || (cifrag->CIid == NULLINDEX)) {
      fprintf(stderr, "ERROR:  Frag %d has null cid or CIid.  Fragment is not in an input unitig!\n", i);
      numErrors++;
    }
  }

  fprintf(stderr,"Processed %d unitigs with %d fragments.\n",
          numUTG, numFRG);

  if (numErrors > 0)
    fprintf(stderr, "ERROR:  Some fragments are not in unitigs.\n");
  assert(numErrors == 0);

  ScaffoldGraph->numLiveCIs     = GetNumGraphNodes(ScaffoldGraph->CIGraph);
  ScaffoldGraph->numOriginalCIs = GetNumGraphNodes(ScaffoldGraph->CIGraph);

  return(0);
}





void
ProcessInputUnitig(MultiAlignT *uma) {
  CDS_CID_t       cfr;
  ChunkInstanceT  CI;

  int32           length = GetMultiAlignUngappedLength(uma);

  memset(&CI, 0, sizeof(ChunkInstanceT));

  CI.id                                  = uma->maID;
  CI.bpLength.mean                       = length;
  CI.bpLength.variance                   = MAX(1.0,ComputeFudgeVariance(CI.bpLength.mean));
  CI.edgeHead                            = NULLINDEX;
  CI.setID                               = NULLINDEX;
  CI.scaffoldID                          = NULLINDEX;
  CI.indexInScaffold                     = NULLINDEX;
  CI.prevScaffoldID                      = NULLINDEX;
  CI.numEssentialA                       = 0;
  CI.numEssentialB                       = 0;
  CI.essentialEdgeA                      = NULLINDEX;
  CI.essentialEdgeB                      = NULLINDEX;
  CI.smoothExpectedCID                   = NULLINDEX;
  CI.BEndNext                            = NULLINDEX;
  CI.AEndNext                            = NULLINDEX;

  if (ScaffoldGraph->tigStore->getUnitigCoverageStat(uma->maID) < -1000)
    ScaffoldGraph->tigStore->setUnitigCoverageStat(uma->maID, -1000);

  CI.info.CI.contigID                    = NULLINDEX;
  CI.info.CI.numInstances                = 0;
  CI.info.CI.instances.in_line.instance1 = 0;
  CI.info.CI.instances.in_line.instance2 = 0;
  CI.info.CI.instances.va                = NULL;
  CI.info.CI.source                      = NULLINDEX;

  CI.flags.all                           = 0;

  CI.offsetAEnd.mean                     = 0.0;
  CI.offsetAEnd.variance                 = 0.0;
  CI.offsetBEnd                          = CI.bpLength;

  int  isUnique     = TRUE;

  if (GetNumIntMultiPoss(uma->f_list) < CGW_MIN_READS_IN_UNIQUE)
    isUnique = FALSE;

  if (ScaffoldGraph->tigStore->getUnitigCoverageStat(uma->maID) < GlobalData->cgbUniqueCutoff)
    isUnique = FALSE;

#if 0
  //  This is an attempt to not blindly call all short unitigs as non-unique.  It didn't work so
  //  well in initial limited testing.
  if ((ScaffoldGraph->tigStore->getUnitigCoverageStat(uma->maID) < GlobalData->cgbDefinitelyUniqueCutoff) &&
      (length < CGW_MIN_DISCRIMINATOR_UNIQUE_LENGTH))
    isUnique = FALSE;
#else
  if (length < CGW_MIN_DISCRIMINATOR_UNIQUE_LENGTH)
    isUnique = FALSE;
#endif

  //  MicroHet probability is actually the probability of the sequence being UNIQUE, based on
  //  microhet considerations.  Falling below threshhold makes something a repeat.
  //  Note that this is off by default (see options -e, -i)
  if ((isUnique) &&
      (ScaffoldGraph->tigStore->getUnitigMicroHetProb(uma->maID) < GlobalData->cgbMicrohetProb) &&
      (ScaffoldGraph->tigStore->getUnitigCoverageStat(uma->maID) < GlobalData->cgbApplyMicrohetCutoff))
    isUnique = FALSE;

  // allow flag to overwrite what the default behavior for a chunk and force it to be unique or repeat

  if (ScaffoldGraph->tigStore->getUnitigFUR(CI.id) == AS_FORCED_UNIQUE)
    isUnique = TRUE;
  else if (ScaffoldGraph->tigStore->getUnitigFUR(CI.id) == AS_FORCED_REPEAT)
    isUnique = FALSE;

  if (isUnique) {
    ScaffoldGraph->numDiscriminatorUniqueCIs++;
    CI.flags.bits.isUnique = 1;
    CI.type                = DISCRIMINATORUNIQUECHUNK_CGW;
  } else {
    CI.flags.bits.isUnique = 0;
    CI.type                = UNRESOLVEDCHUNK_CGW;
  }

  CI.flags.bits.smoothSeenAlready = FALSE;
  CI.flags.bits.isCI              = TRUE;
  CI.flags.bits.isChaff           = FALSE;
  CI.flags.bits.isClosure         = FALSE;

  for(cfr = 0; cfr < GetNumIntMultiPoss(uma->f_list); cfr++){
    IntMultiPos *imp    = GetIntMultiPos(uma->f_list, cfr);
    CIFragT     *cifrag = GetCIFragT(ScaffoldGraph->CIFrags, imp->ident);

    cifrag->cid  = uma->maID;
    cifrag->CIid = uma->maID;
  }

  //  Singleton chunks are chaff; singleton frags are chaff unless proven otherwise

  if (GetNumIntMultiPoss(uma->f_list) < 2) {
    IntMultiPos *imp    = GetIntMultiPos(uma->f_list, 0);
    CIFragT     *cifrag = GetCIFragT(ScaffoldGraph->CIFrags, imp->ident);

    CI.flags.bits.isChaff          = TRUE;
    cifrag->flags.bits.isSingleton = TRUE;
    cifrag->flags.bits.isChaff     = TRUE;
  }

  // Insert the Chunk Instance

  SetChunkInstanceT(ScaffoldGraph->CIGraph->nodes, CI.id, &CI);

  // Mark all frags as being members of this CI, and set their offsets within the CI
  // mark unitigs and contigs
  UpdateNodeFragments(ScaffoldGraph->CIGraph,CI.id, CI.type == DISCRIMINATORUNIQUECHUNK_CGW, TRUE);
}




void
LoadDistData(void) {
  int32 numDists = ScaffoldGraph->gkpStore->gkStore_getNumLibraries();
  CDS_CID_t i;

  for(i = 1; i <= numDists; i++){
    DistT dist;
    gkLibrary  *gkpl = ScaffoldGraph->gkpStore->gkStore_getLibrary(i);

    dist.mu             = gkpl->mean;
    dist.sigma          = gkpl->stddev;
    dist.numSamples     = 0;
    dist.min            = INT32_MAX;
    dist.max            = INT32_MIN;
    dist.bnum           = 0;
    dist.bsize          = 0;
    dist.histogram      = NULL;
    dist.lower          = dist.mu - CGW_CUTOFF * dist.sigma;
    dist.upper          = dist.mu + CGW_CUTOFF * dist.sigma;
    dist.numBad         = 0;
    dist.allowUpdate    = (gkpl->constantInsertSize == false);

    fprintf(stderr,"* Loaded dist %s,"F_CID" (%g +/- %g)\n",
            AS_UID_toString(gkpl->libraryUID), i, dist.mu, dist.sigma);

    SetDistT(ScaffoldGraph->Dists, i, &dist);
  }
}
