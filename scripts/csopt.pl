#!/usr/bin/perl -w

use strict;
use warnings;
use Getopt::Long;
use Pod::Usage;
use Template;
use My::Utils qw(filename);
use File::Spec::Functions;
use Cwd qw(abs_path);


=pod
=head1 NAME
  
  csopt.pl - Optimizes the admixture coefficient tau for a given model.

=head1 SYNOPSIS

  csopt.pl [OPTIONS] -m MODEL

  OPTIONS:
    -m, --model FILE        The model used for optimization
    -o, --out-dir DIR       The output directory
    -d, --db STRING         The name of the database [def: scop20_1.73_opt]
    -j, --iters [1;inf[     Maximum number of iterations to use in CSI-BLAST [def: 1]
    -x, --pc-admix ]0;1]    Optimize pseudocounts admixture coefficient x [def: off]
    -z, --pc-neff [1;inf[   Optimize pseudocounts admixture coefficient z [def: 12.0]
    -c, --pc-ali [0;inf[    Constant for alignment pseudocounts in CSI-BLAST [def: 14.0]
    -p, --params STRING     Additional parameters
    -n, --niter [1;inf[     Number of iterations with Newtons method [def: 2]
    -s, --[no-]submit       Submit csopt job
        --seed INT          Seed for csopt
    -h, --help              Print this message

=head1 AUTHOR

  Angermueller Christof
  angermue@in.tum.de

=cut


### Variables ###


my %tplvars = (
  model   => undef,
  outdir  => undef,
  db      => "scop20_1.73_opt",
  e       => "1e5",
  v       => 1000,
  b       => 0,
  j       => 1,
  h       => "1e-3",
  z       => 12.0,
  x       => undef,
  c       => 14.0,
  n       => 2,
  mult    => undef,
  seed    => int(rand(1e6)),
  csblast => "csblast-$ENV{CSVERSION}",
  ENV     => \%ENV
);
my @mult = (50, 10, 5);
my $submit = 0;

my $runme = q(#!/bin/bash

DB=[% ENV.DBS %]/[% db %]
BASEDIR=[% outdir %]

csopt \
  -p $BASEDIR/csopt.yml \
  -b $BASEDIR/csblast.sh \
  -o $BASEDIR/out \
  -w $BASEDIR/workdir \
  -d $BASEDIR \
  -e ${DB}_db \
  -g "$DB/*.seq" \
  -n [% n %] \
  -r 1 \
  --mult [% mult %] \
  --seed [% seed %]\
  --no-keep \
  --keep-bla
);

my $csopt_yml = "---
[%- IF x %]
x:  
  order:    1
  value:    [% x %]
  add:      0.05
  min:      0.0
  max:      1.0
[%- ELSE %]
z:  
  order:    1
  value:    [% z %]
  add:      0.5
  min:      5.0
  max:      15.0
[%- END %]
[%- IF j > 1 %]

c:
  order:    2
  value:    [% c %]
  add:      2.0
  min:      0.0
  max:      25.0
[%- END %]
";

my $csblast_sh = q(#!/bin/bash

<% model = "[% model %]" %>
<% dirbase = "#{csblastdir}/#{basename}" %>
<% outfile = "#{dirbase}.bla" %>

[% csblast %] \
  -D <%= model %> \
  -i FILENAME \
  -o <%= outfile %> \
  -d <%= seqfile %> \
  --blast-path [% ENV.BLAST_PATH %] \
  [%- IF x %]
  -x <%= x %> \
  [%- ELSE %]
  -z <%= z %> \
  [%- END %]
  -e [% e %] -v [% v %] -b [% b %] [% params %]);

my $csiblast_sh = q(#!/bin/bash

<% model = "[% model %]" %>
<% dirbase = "#{csblastdir}/#{basename}" %>
<% outfile = "#{dirbase}.bla" %>
<% chkfile = "#{dirbase}.chk" %>

[% csblast %] \
  -D <%= model %> \
  -i FILENAME \
  -o <%= outfile %> \
  -C <%= chkfile %> \
  -d [% ENV.DBS %]/nr_2011-12-09/nr \
  --blast-path [% ENV.BLAST_PATH %] \
  [%- IF x %]
  -x <%= x %> \
  [%- ELSE %]
  -z <%= z %> \
  [%- END %]
  -c <%= c %> \
  -e [% e %] -h [% h %] -j [% j %] [% params %]

[% csblast %] \\
  -D <%= model %> \\
  -i FILENAME \\
  -R <%= chkfile %> \\
  -o <%= outfile %> \\
  -d <%= seqfile %> \\
  --blast-path [% ENV.BLAST_PATH %] \\
  [%- IF x %]
  -x <%= x %> \
  [%- ELSE %]
  -z <%= z %> \
  [%- END %]
  -c <%= c %> \
  -e [% e %] -v [% v %] -b [% b %] [% params %]);


### Initialization ###


GetOptions(
  "m|model=s"    => \$tplvars{model},
  "o|out-dir=s"  => \$tplvars{outdir},
  "d|db=s"       => \$tplvars{db},
  "j|iter=i"     => \$tplvars{j},
  "x|pc-admix=f" => \$tplvars{x},
  "z|pc-neff=f"  => \$tplvars{z},
  "p|params=s"   => \$tplvars{params},
  "s|submit!"    => \$submit,
  "n|niter=i"    => \$tplvars{n},
  "seed=i"       => \$tplvars{seed},
  "h|help"       => sub { pod2usage(2); }
) or die pod2usage(1);
unless ($tplvars{model}) { die pod2usage("No model provided!"); }
$tplvars{model} = abs_path($tplvars{model});
unless ($tplvars{outdir}) { die pod2usage("No output directory provided!"); }
if (defined($tplvars{x})) {
  if ($tplvars{x} <= 0.0 || $tplvars{x} > 1.0) { 
    die pod2usage("Value of pc-admix x invalid!");
  }
}
if (system("$tplvars{csblast} &> /dev/null")) {
  die "'$tplvars{csblast}' binary not found!";
}
$tplvars{mult} = $mult[$tplvars{j} - 1];


### Create script files and submit the optimization job ###


my $tpl = Template->new({
    PRE_CHOMP => 0
  });
mkdir $tplvars{outdir};
$tpl->process(\$csopt_yml, \%tplvars, catfile($tplvars{outdir}, "csopt.yml"));
$tpl->process(($tplvars{j} > 1 ? \$csiblast_sh : \$csblast_sh), \%tplvars, catfile($tplvars{outdir}, "csblast.sh"));
my $run= catfile($tplvars{outdir}, "RUNME");
$tpl->process(\$runme, \%tplvars, $run);
system("chmod u+x $run");
if ($submit) { system("qsub -N csopt -o $tplvars{outdir}/log $run"); }
