#ifndef SNARK_POINT_CLOUD_SPHERICAL_GRID_H_
#define SNARK_POINT_CLOUD_SPHERICAL_GRID_H_

#include <boost/multi_array.hpp>
#include <snark/math/range_bearing_elevation.h>

namespace snark {

/// a simple 2d bearing-elevation grid
struct bearing_elevation_grid
{
    /// index
    class index
    {
        public:
            /// index type
            typedef boost::array< std::size_t, 2 > type;

            /// constructors
            index( double bearing_begin, double elevation_begin, double bearing_resolution, double elevation_resolution );
            index( double bearing_begin, double elevation_begin, double resolution );
            index( double bearing_resolution, double elevation_resolution );
            index( double resolution );
            index( const snark::bearing_elevation& begin, const snark::bearing_elevation& resolution );

            /// constructor for full spherical grid with given resolution (range ignored)
            index( const snark::bearing_elevation& resolution );

            /// @return index relative to begin with given resolution
            type operator()( double bearing, double elevation ) const;

            /// @return index relative to begin with given resolution
            type operator()( const snark::bearing_elevation& v ) const;

            /// @return bearing, elevation for given index (range will be set to 1)
            snark::bearing_elevation bearing_elevation( const type& i ) const;

            /// @return begin
            const snark::bearing_elevation& begin() const;

            /// @return resolution
            const snark::bearing_elevation& resolution() const;

        private:
            snark::bearing_elevation begin_;
            snark::bearing_elevation resolution_;
    };

    /// a convenience wrapper around boost::multi_array
    /// nothing prevents you from using other containers
    /// in conjunction with bearing_elevation_index
    template < typename T >
    class type : public boost::multi_array< T, 2 >
    {
        public:
            /// types
            typedef typename bearing_elevation_grid::index index_t;
            typedef boost::multi_array< T, 2 > base_t;

            /// constructors
            type( const index_t& i, const snark::bearing_elevation& end ) : base_t( boost::extents[ i( end )[0] ][ i( end )[1] ] ), index_( i ) {}
            type( const index_t& i, const typename index_t::type size ) : base_t( boost::extents[ size[0] ][ size[1] ] ), index_( i ) {}
            type( const index_t& i, std::size_t bearing_size, std::size_t elevation_size ) : base_t( boost::extents[ bearing_size ][ elevation_size ] ), index_( i ) {}

            /// accessors
            T& operator()( double bearing, double elevation ) { return this->base_t::operator()( index_( bearing, elevation ) ); }
            const T& operator()( double bearing, double elevation ) const { return this->base_t::operator()( index_( bearing, elevation ) ); }

            /// @return index
            const index_t& index() const { return index_; }

        private:
            index_t index_;
    };
};

/// spherical grid based on range-bearing-elevation coordinates
/// @todo implement based on voxel grid + bearing_elevation_grid
template < typename T >
class spherical_grid;

} // namespace snark {

#endif // SNARK_POINT_CLOUD_SPHERICAL_GRID_H_
