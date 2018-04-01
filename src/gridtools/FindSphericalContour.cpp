/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2016-2018 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "core/ActionRegister.h"
#include "ContourFindingBase.h"
#include "tools/Random.h"

//+PLUMEDOC GRIDANALYSIS FIND_SPHERICAL_CONTOUR
/*
Find an isocontour in a three dimensional grid by searching over a Fibonacci sphere.

As discussed in the part of the manual on \ref Analysis PLUMED contains a number of tools that allow you to calculate
a function on a grid.  The function on this grid might be a \ref HISTOGRAM as a function of a few collective variables
or it might be a phase field that has been calcualted using \ref MULTICOLVARDENS.  If this function has one or two input
arguments it is relatively straightforward to plot the function.  If by contrast the data has a three dimensions it can be
difficult to visualize.

This action provides one tool for visualizing these functions.  It can be used to search for a set of points on a contour
wher the function takes a particular value.  In other words, for the function \f$f(x,y,z)\f$ this action would find a set
of points \f$\{x_c,y_c,z_c\}\f$ that have:

\f[
f(x_c,y_c,z_c) - c = 0
\f]

where \f$c\f$ is some constant value that is specified by the user.  The points on this contour are find by searching along a
set of equally spaced radii of a sphere that centered at on particular, user-speciified atom or virtual atom.  To ensure that
these search radii are equally spaced on the surface of the sphere the search directions are generated by using a fibonacci
spiral projected on a sphere.  In other words, the search directions are given by:

\f[
\mathbf{r}_i = \left(
\begin{matrix}
\sqrt{1 - y^2} \cos(\phi) \\
\frac{2i}{n} - 1 + \frac{1}{n}  \\
\sqrt{1 - y^2} \sin(\phi)
\end{matrix}
\right)
\f]

where \f$y\f$ is the quantity second component of the vector defined above, \f$n\f$ is the number of directions to look in and \f$\phi\f$ is

\f[
\phi = \mod(i + R, n) \pi ( 3 - \sqrt{5} )
\f]

where \f$R\f$ is a random variable between 0 and \f$n-1\f$ that is generated during the read in of the input file and that is fixed during
the whole calculation.

It is important to note that this action can only be used to detect countours in three dimensional functions.  In addition, this action will fail to
find the full set of contour  points if the contour does not have the same topology as a sphere.  If you are uncertain that the isocontours in your
function have a spherical topology you should use \ref FIND_CONTOUR in place of \ref FIND_SPHERICAL_CONTOUR.

\par Examples

The following input demonstrates how this action can be used.  The input here is used to study the shape of a droplet that has been formed during the
condensation of Lennard Jones from the vapour.  The input below achieves this by calculating the coordination numbers of all the atoms within the gas.
Obviously, those atoms within the droplet will have a large value for the coordination number while the isolated atoms in the gas will have a low value.
As such we can detect the sizes of the droplets by constructing a \ref CONTACT_MATRIX whose \f$ij\f$ element tells us whether atom \f$i\f$ and atom \f$j\f$
have coordination number that is greater that two.  The atoms within the various droplets within the system can then be found by performing a
\ref DFSCLUSTERING on this matrix to detect the connected components.  We can take the largest of these connected components and find the center of the droplet
by exploiting the functionality within \ref CENTER_OF_MULTICOLVAR.  We can then construct a phase field based on the positions of the atoms in the largest
cluster and the values of the coordination numbers of these atoms.  The final line in the input then finds the a set of points on the dividing surface that separates
teh droplet from the surrounding gas.  The value of the phase field on this isocontour is equal to 0.75.

\plumedfile
# Calculate coordination numbers
c1: COORDINATIONNUMBER SPECIES=1-512 SWITCH={EXP D_0=4.0 R_0=0.5 D_MAX=6.0}
# Select coordination numbers that are more than 2.0
cf: MFILTER_MORE DATA=c1 SWITCH={RATIONAL D_0=2.0 R_0=0.1} LOWMEM
# Build a contact matrix
mat: CONTACT_MATRIX ATOMS=cf SWITCH={EXP D_0=4.0 R_0=0.5 D_MAX=6.0}
# Find largest cluster
dfs: DFSCLUSTERING MATRIX=mat
clust1: CLUSTER_PROPERTIES CLUSTERS=dfs CLUSTER=1
# Find center of largest cluster
trans1: MTRANSFORM_MORE DATA=clust1 SWITCH={RATIONAL D_0=2.0 R_0=0.1} LOWMEM
cent: CENTER_OF_MULTICOLVAR DATA=trans1
# Calculate the phase field of the coordination
dens: MULTICOLVARDENS DATA=trans1 ORIGIN=cent DIR=xyz NBINS=30,30,30 BANDWIDTH=2.0,2.0,2.0
# Find the isocontour around the nucleus
FIND_SPHERICAL_CONTOUR GRID=dens CONTOUR=0.85 INNER_RADIUS=10.0 OUTER_RADIUS=40.0 FILE=mysurface.xyz UNITS=A PRECISION=4 NPOINTS=100
\endplumedfile

*/
//+ENDPLUMEDOC

namespace PLMD {
namespace gridtools {

class FindSphericalContour : public ContourFindingBase {
private:
  unsigned nbins, npoints;
  double min, max;
  GridCoordinatesObject gridcoords;
public:
  static void registerKeywords( Keywords& keys );
  explicit FindSphericalContour(const ActionOptions&ao);
  void finishOutputSetup() {}
  // Make these two so we can output a two dimensional analogue of the grid one day
  void getInfoForGridHeader( std::string& gtype, std::vector<std::string>& argn, std::vector<std::string>& min,
                             std::vector<std::string>& max, std::vector<unsigned>& nbin,
                             std::vector<double>& spacing, std::vector<bool>& pbc, const bool& dumpcube ) const ;
  void getGridPointIndicesAndCoordinates( const unsigned& ind, std::vector<unsigned>& indices, std::vector<double>& coords ) const { plumed_error(); }
  void getGridPointAsCoordinate( const unsigned& ind, const bool& setlength, std::vector<double>& coords ) const ;
  unsigned getNumberOfDerivatives() const ;
  void buildCurrentTaskList( std::vector<unsigned>& tflags );
  void performTask( const unsigned& current, MultiValue& myvals ) const ;
  void gatherGridAccumulators( const unsigned& code, const MultiValue& myvals,
                               const unsigned& bufstart, std::vector<double>& buffer ) const ;
};

PLUMED_REGISTER_ACTION(FindSphericalContour,"FIND_SPHERICAL_CONTOUR")

void FindSphericalContour::registerKeywords( Keywords& keys ) {
  ContourFindingBase::registerKeywords( keys );
  keys.add("compulsory","NPOINTS","the number of points for which we are looking for the contour");
  keys.add("compulsory","INNER_RADIUS","the minimum radius on which to look for the contour");
  keys.add("compulsory","OUTER_RADIUS","the outer radius on which to look for the contour");
  keys.add("compulsory","NBINS","1","the number of discrete sections in which to divide the distance between the inner and outer radius when searching for a contour");
}

FindSphericalContour::FindSphericalContour(const ActionOptions&ao):
  Action(ao),
  ContourFindingBase(ao)
{
  if( getPntrToArgument(0)->getRank()!=3 ) error("input grid must be three dimensional");

  parse("NPOINTS",npoints); log.printf("  searching for %u points on dividing surface \n",npoints);
  parse("INNER_RADIUS",min); parse("OUTER_RADIUS",max); parse("NBINS",nbins);
  log.printf("  expecting to find dividing surface at radii between %f and %f \n",min,max);
  log.printf("  looking for contour in windows of length %f \n", (max-min)/nbins);
  // Set this here so the same set of grid points are used on every turn
  std::vector<bool> ipbc( 3, false ); gridcoords.setup( "fibonacci", ipbc, npoints, 0.0 );
  // Now create a value
  std::vector<unsigned> shape( 3 ); shape[0]=npoints; shape[1]=shape[2]=1;
  addValueWithDerivatives( shape ); setNotPeriodic(); checkRead();
  // Create a task list
  for(unsigned i=0; i<npoints; ++i) addTaskToList( i );
}

void FindSphericalContour::getInfoForGridHeader( std::string& gtype, std::vector<std::string>& argn, std::vector<std::string>& min,
    std::vector<std::string>& max, std::vector<unsigned>& nbin,
    std::vector<double>& spacing, std::vector<bool>& pbc, const bool& dumpcube ) const {
  gtype="fibonacci"; nbin[0] = npoints; nbin[1]=nbin[2]=1; spacing[0]=0.0;
}

void FindSphericalContour::getGridPointAsCoordinate( const unsigned& ind, const bool& setlength, std::vector<double>& coords ) const {
  if( setlength ) gridcoords.putCoordinateAtValue( ind, getPntrToOutput(0)->get(ind), coords );
  else  gridcoords.putCoordinateAtValue( ind, 1.0, coords );
}

unsigned FindSphericalContour::getNumberOfDerivatives() const {
  return gridcoords.getDimension();
}

void FindSphericalContour::buildCurrentTaskList( std::vector<unsigned>& tflags ) {
  tflags.assign(tflags.size(),1);
}

void FindSphericalContour::performTask( const unsigned& current, MultiValue& myvals ) const {
  // Generate contour point on inner sphere
  std::vector<double> contour_point(3), direction(3), der(3), tmp(3);
  // Retrieve this contour point from grid
  gridcoords.getGridPointCoordinates( current, direction );
  // Now setup contour point on inner sphere
  for(unsigned j=0; j<3; ++j) {
    contour_point[j] = min*direction[j];
    direction[j] = (max-min)*direction[j] / static_cast<double>(nbins);
  }

  bool found=false;
  for(unsigned k=0; k<nbins; ++k) {
    for(unsigned j=0; j<3; ++j) tmp[j] = contour_point[j] + direction[j];
    double val1 = getDifferenceFromContour( contour_point, der );
    double val2 = getDifferenceFromContour( tmp, der );
    if( val1*val2<0 ) {
      findContour( direction, contour_point );
      double norm=0; for(unsigned j=0; j<3; ++j) norm += contour_point[j]*contour_point[j];
      myvals.setValue( getPntrToOutput(0)->getPositionInStream(), sqrt(norm) ); found=true; break;
    }
    for(unsigned j=0; j<3; ++j) contour_point[j] = tmp[j];
  }
  if( !found ) error("range does not bracket the dividing surface");
}

void FindSphericalContour::gatherGridAccumulators( const unsigned& code, const MultiValue& myvals,
    const unsigned& bufstart, std::vector<double>& buffer ) const {
  unsigned istart = bufstart + (1+getNumberOfDerivatives())*code;
  unsigned valout = getPntrToOutput(0)->getPositionInStream(); buffer[istart] += myvals.get( valout );
}

}
}
