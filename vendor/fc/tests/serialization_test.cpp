#include <boost/test/unit_test.hpp>
#include <fc/log/logger.hpp>

#include <fc/container/flat.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>

namespace fc { namespace test {

   struct item;
   inline bool operator < ( const item& a, const item& b );
   inline bool operator == ( const item& a, const item& b );

   struct item_wrapper
   {
      item_wrapper() {}
      item_wrapper(item&& it) { v.reserve(1); v.insert( it ); }
      boost::container::flat_set<struct item> v;
   };
   inline bool operator < ( const item_wrapper& a, const item_wrapper& b );
   inline bool operator == ( const item_wrapper& a, const item_wrapper& b );

   struct item
   {
      item(int32_t lvl = 0) : level(lvl) {}
      item(item_wrapper&& wp, int32_t lvl = 0) : level(lvl), w(wp) {}
      int32_t      level;
      item_wrapper w;
   };

   inline bool operator == ( const item& a, const item& b )
   { return ( std::tie( a.level, a.w ) == std::tie( b.level, b.w ) ); }

   inline bool operator < ( const item& a, const item& b )
   { return ( std::tie( a.level, a.w ) < std::tie( b.level, b.w ) ); }

   inline bool operator == ( const item_wrapper& a, const item_wrapper& b )
   { return ( std::tie( a.v ) == std::tie( b.v ) ); }

   inline bool operator < ( const item_wrapper& a, const item_wrapper& b )
   { return ( std::tie( a.v ) < std::tie( b.v ) ); }

} }

FC_REFLECT( fc::test::item_wrapper, (v) );
FC_REFLECT( fc::test::item, (level)(w) );

// These are used in the custom_serialization test

static bool custom_serialization_used = false;
// Custom serialized type with a fake serializer that does nothing but set the above bool to true
struct custom_serialized_type {
   int a = 1;
   std::string b = "Hi!";
   bool c = true;

   // Override default serialization for reflected types
   template<typename Stream>
   void fc_pack(Stream&, uint32_t) const {
      custom_serialization_used = true;
   }
   template<typename Stream>
   void fc_unpack(Stream&, uint32_t) {
      custom_serialization_used = true;
   }

   void fc_to_variant(fc::variant&, uint32_t) const {
      custom_serialization_used = true;
   }
   void fc_from_variant(const fc::variant&, uint32_t) {
      custom_serialization_used = true;
   }
};

// Default-serialized type with auxiliary data to serialize
struct auxiliary_serialized_type {
   int a = 1;
   std::string b = "Hi!";
   bool c = true;

   // Define auxiliary data
   template<typename Stream>
   void fc_pack_auxiliary(Stream& s, uint32_t) const {
      fc::raw::pack(s, 7);
      custom_serialization_used = true;
   }
   template<typename Stream>
   void fc_unpack_auxiliary(Stream& s, uint32_t) {
      int data;
      fc::raw::unpack(s, data);
      BOOST_CHECK_EQUAL(data, 7);
      custom_serialization_used = true;
   }

   void fc_auxiliary_to_variant(fc::variant& v, uint32_t) const {
      v = 7;
      custom_serialization_used = true;
   }
   void fc_auxiliary_from_variant(const fc::variant& v, uint32_t) {
      BOOST_CHECK_EQUAL(v.as_int64(), 7);
      custom_serialization_used = true;
   }
};

FC_REFLECT(custom_serialized_type, (a)(b)(c))
FC_REFLECT(auxiliary_serialized_type, (a)(b)(c))

BOOST_AUTO_TEST_SUITE(fc_serialization)

BOOST_AUTO_TEST_CASE( nested_objects_test )
{ try {

   auto create_nested_object = []( uint32_t level )
   {
      ilog( "Creating nested object with ${lv} level(s)", ("lv",level) );
      fc::test::item nested;
      for( uint32_t i = 1; i <= level; i++ )
      {
         if( i % 100 == 0 )
            ilog( "Creating level ${lv}", ("lv",i) );
         fc::test::item_wrapper wp( std::move(nested) );
         nested = fc::test::item( std::move(wp), i );
      }
      return nested;
   };

   // 100 levels, should be allowed
   {
      auto nested = create_nested_object( 100 );

      std::stringstream ss;

      BOOST_TEST_MESSAGE( "About to pack." );
      fc::raw::pack( ss, nested );

      BOOST_TEST_MESSAGE( "About to unpack." );
      fc::test::item unpacked;
      fc::raw::unpack( ss, unpacked );

      BOOST_CHECK( unpacked == nested );
   }

   // 150 levels, by default packing will fail
   {
      auto nested = create_nested_object( 150 );

      std::stringstream ss;

      BOOST_TEST_MESSAGE( "About to pack." );
      BOOST_CHECK_THROW( fc::raw::pack( ss, nested ), fc::assert_exception );
   }

   // 150 levels and allow packing, unpacking will fail
   {
      auto nested = create_nested_object( 150 );

      std::stringstream ss;

      BOOST_TEST_MESSAGE( "About to pack." );
      fc::raw::pack( ss, nested, 1500 );

      BOOST_TEST_MESSAGE( "About to unpack." );
      fc::test::item unpacked;
      BOOST_CHECK_THROW( fc::raw::unpack( ss, unpacked ), fc::assert_exception );
   }

} FC_CAPTURE_LOG_AND_RETHROW ( (0) ) }

BOOST_AUTO_TEST_CASE( unpack_recursion_test )
{
   try
   {
      std::stringstream ss;
      int recursion_level = 100000;
      uint64_t allocation_per_level = 500000;

      for ( int i = 0; i < recursion_level; i++ )
      {
         fc::raw::pack( ss, fc::unsigned_int( allocation_per_level ) );
         fc::raw::pack( ss, static_cast< uint8_t >( fc::variant::array_type ) );
      }

      std::vector< fc::variant > v;
      BOOST_REQUIRE_THROW( fc::raw::unpack( ss, v ), fc::assert_exception );
   }
   FC_LOG_AND_RETHROW();
}

BOOST_AUTO_TEST_CASE( custom_serialization )
{ try {
   custom_serialization_used = false;
   // Create our custom serialized type and serialize it; check that only our fake serializer ran
   custom_serialized_type obj;
   auto packed = fc::raw::pack(obj);
   BOOST_CHECK(packed.empty());
   BOOST_CHECK(custom_serialization_used);

   // Deserializer won't run on an empty vector; put a byte in so deserialization tests can run
   packed = {'a'};

   // Reset the flag, alter the object, and deserialize it, again checking that only the fake serializer ran
   custom_serialization_used = false;
   obj.c = false;
   fc::raw::unpack(packed, obj);
   BOOST_CHECK(obj.c == false);
   BOOST_CHECK(custom_serialization_used);

   // Reset the flag, deserialize again without existing ref, check if fake serializer ran
   custom_serialization_used = false;
   auto obj2 = fc::raw::unpack<custom_serialized_type>(packed);
   BOOST_CHECK(custom_serialization_used);

   // All over again, but for variants instead of binary serialization
   fc::variant v(obj, 10);
   BOOST_CHECK(v.is_null());
   BOOST_CHECK(custom_serialization_used);
   custom_serialization_used = false;
   fc::to_variant(obj, v, 10);
   BOOST_CHECK(custom_serialization_used);
   // JSON should be affected too. Check it is.
   custom_serialization_used = false;
   BOOST_CHECK_EQUAL(fc::json::to_string(obj), "null");
   BOOST_CHECK(custom_serialization_used);

   custom_serialization_used = false;
   obj.c = true;
   fc::from_variant(v, obj, 10);
   BOOST_CHECK(custom_serialization_used);
   BOOST_CHECK(obj.c);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( auxiliary_serialization )
{ try {
   custom_serialization_used = false;
   // Just serialize and deserialize, text and binary. The class does the checks itself -- we just make sure they ran.
   auxiliary_serialized_type obj;
   auto packed = fc::raw::pack(obj);
   BOOST_CHECK(custom_serialization_used);
   custom_serialization_used = false;
   obj.a = 0;
   obj.b = "Something else";
   obj.c = false;
   // Why I need this cast to get it to build, I may never know... Nor will I get that hour of my life back.
   fc::raw::unpack((const std::vector<char>&)packed, obj);
   BOOST_CHECK(custom_serialization_used);
   BOOST_CHECK_EQUAL(obj.a, 1);
   BOOST_CHECK_EQUAL(obj.b, "Hi!");
   BOOST_CHECK_EQUAL(obj.c, true);
   custom_serialization_used = false;

   fc::variant v;
   fc::to_variant(obj, v, 10);
   BOOST_CHECK(custom_serialization_used);
   custom_serialization_used = false;
   obj.a = 0;
   obj.b = "Something else";
   obj.c = false;
   fc::from_variant(v, obj, 10);
   BOOST_CHECK(custom_serialization_used);
   BOOST_CHECK_EQUAL(obj.a, 1);
   BOOST_CHECK_EQUAL(obj.b, "Hi!");
   BOOST_CHECK_EQUAL(obj.c, true);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
