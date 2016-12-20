#!/bin/sh

name=$1

dnadiff -p $name.col0.dnadiff \
  /data/references/arabidopsis_thaliana/arabidopsis_thaliana_col0.fasta \
  $name.contigs.fasta

mummerplot --fat -t png -p $name.col0.dnadiff $name.col0.dnadiff.delta

rm -f $name.col0.dnadiff.1coords
rm -f $name.col0.dnadiff.1delta
rm -f $name.col0.dnadiff.mcoords
rm -f $name.col0.dnadiff.mdelta
rm -f $name.col0.dnadiff.qdiff
rm -f $name.col0.dnadiff.rdiff

rm -f $name.col0.dnadiff.filter
rm -f $name.col0.dnadiff.rplot
rm -f $name.col0.dnadiff.fplot
rm -f $name.col0.dnadiff.gp



dnadiff -p $name.ler0.dnadiff \
  /data/references/arabidopsis_thaliana/arabidopsis_thaliana_ler0.v7.fasta \
  $name.contigs.fasta

mummerplot --fat -t png -p $name.ler0.dnadiff $name.ler0.dnadiff.delta

rm -f $name.ler0.dnadiff.1coords
rm -f $name.ler0.dnadiff.1delta
rm -f $name.ler0.dnadiff.mcoords
rm -f $name.ler0.dnadiff.mdelta
rm -f $name.ler0.dnadiff.qdiff
rm -f $name.ler0.dnadiff.rdiff

rm -f $name.ler0.dnadiff.filter
rm -f $name.ler0.dnadiff.rplot
rm -f $name.ler0.dnadiff.fplot
rm -f $name.ler0.dnadiff.gp
