#!/usr/bin/perl -w

use strict;
use Getopt::Long "GetOptions";
use FindBin qw($RealBin);

my $___FACTOR_DELIMITER = "|";

# utilities
my $ZCAT = "gzip -cd";
my $BZCAT = "bzcat";

my ($CORPUS,$REDUCED,$FACTOR);
die("ERROR: wrong syntax when invoking reduce-factors")
    unless &GetOptions('corpus=s' => \$CORPUS,
                       'reduced-corpus=s' => \$REDUCED,
		       'factor=s' => \$FACTOR);

&reduce_factors($CORPUS,$REDUCED,$FACTOR);

# from train-model.perl
sub reduce_factors {
    my ($full,$reduced,$factors) = @_;

    my @INCLUDE = sort {$a <=> $b} split(/,/,$factors);

    print "Reducing factors to produce $reduced  @ ".`date`;
    while(-e $reduced.".lock") {
	sleep(10);
    }
    if (-e $reduced) {
        print STDERR "  $reduced in place, reusing\n";
        return;
    }
    if (-e $reduced.".gz") {
        print STDERR "  $reduced.gz in place, reusing\n";
        return;
    }

    # peek at input, to check if we are asked to produce exactly the
    # available factors
    my $inh = open_or_zcat($full);
    my $firstline = <$inh>;
    die "Corpus file $full is empty" unless $firstline;
    close $inh;
    # pick first word
    $firstline =~ s/^\s*//;
    $firstline =~ s/\s.*//;
    # count factors
    my $maxfactorindex = $firstline =~ tr/|/|/;
    if (join(",", @INCLUDE) eq join(",", 0..$maxfactorindex)) {
	# create just symlink; preserving compression
	my $realfull = $full;
	if (!-e $realfull && -e $realfull.".gz") {
            $realfull .= ".gz";
            $reduced =~ s/(\.gz)?$/.gz/;
	}
	safesystem("ln -s '$realfull' '$reduced'")
            or die "Failed to create symlink $realfull -> $reduced";
	return;
    }

    # The default is to select the needed factors
    `touch $reduced.lock`;
    *IN = open_or_zcat($full);
    open(OUT,">".$reduced) or die "ERROR: Can't write $reduced";
    my $nr = 0;
    while(<IN>) {
        $nr++;
        print STDERR "." if $nr % 10000 == 0;
        print STDERR "($nr)" if $nr % 100000 == 0;
	chomp; s/ +/ /g; s/^ //; s/ $//;
	my $first = 1;
	foreach (split) {
	    my @FACTOR = split /\Q$___FACTOR_DELIMITER/;
              # \Q causes to disable metacharacters in regex
	    print OUT " " unless $first;
	    $first = 0;
	    my $first_factor = 1;
            foreach my $outfactor (@INCLUDE) {
              print OUT "|" unless $first_factor;
              $first_factor = 0;
              my $out = $FACTOR[$outfactor];
              die "ERROR: Couldn't find factor $outfactor in token \"$_\" in $full LINE $nr" if !defined $out;
              print OUT $out;
            }
	} 
	print OUT "\n";
    }
    print STDERR "\n";
    close(OUT);
    close(IN);
    `rm -f $reduced.lock`;
}

sub open_or_zcat {
  my $fn = shift;
  my $read = $fn;
  $fn = $fn.".gz" if ! -e $fn && -e $fn.".gz";
  $fn = $fn.".bz2" if ! -e $fn && -e $fn.".bz2";
  if ($fn =~ /\.bz2$/) {
      $read = "$BZCAT $fn|";
  } elsif ($fn =~ /\.gz$/) {
      $read = "$ZCAT $fn|";
  }
  my $hdl;
  open($hdl,$read) or die "Can't read $fn ($read)";
  return $hdl;
}
