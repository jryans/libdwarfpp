/* A dieset-like thing which *doesn't* store DIEs, just retrieves them
 * on-demand. What it needs:
 *
 * find() on offset
 * begin(), end() 
 * ... returning something that can be implicitly constructed 
 *     from a std::map<Dwarf_Off, ...>::iterator
 * operator[] on offset
 * *not* insert
 * a factory-like thing (spec-dependent)
 * all_compile_units() implemented over the top */

#ifndef __DWARFPP_ADT_HPP
#define __DWARFPP_ADT_HPP

#include <functional>
#include <iterator>
#include <limits>
#include <boost/shared_ptr.hpp>
#include <boost/iterator_adaptors.hpp>
#include <boost/make_shared.hpp>
#include "lib.hpp"
#include "expr.hpp"
#include "spec_adt.hpp"
#include "attr.hpp"

namespace dwarf
{
	namespace lib
    {
		using boost::dynamic_pointer_cast;
		using boost::shared_ptr;
		using dwarf::spec::opt; // FIXME: put this in a different namespace
	
    	typedef spec::abstract_dieset abstract_dieset;
        class dieset;
        class basic_die : public die, public virtual spec::basic_die
		// HACK: should really be *private* here, but first we need to forward the stuff
		// like hasattr that is used in the accessors generated by the macros below.
        {
			friend class dieset; // for constructor access
			
        /*protected:*/ public: // FIXME: this is a temporary hack!
        	//boost::shared_ptr<lib::die> p_d;
        protected:
            dieset *p_ds;
            Dwarf_Off m_parent_offset; // FIXME: change to path?
			
			// construct "exactly this" DIE
			basic_die(const lib::die& d, dieset& ds)
			 : lib::die(d), p_ds(&ds), 
			   m_parent_offset(0UL) // will fill lazily if not a CU DIE
			{}
			
			// toplevel special constructor
			// -- we leave the libdwarf Die null, using the lib::die default constructor
			// -- we do get a lib::file reference, from the dieset itself
			basic_die(dieset& ds);
        public:
        	//basic_die(dieset& ds, boost::shared_ptr<lib::die> p_d);
			// We now replicate the lib::die constructors here
			
			// "next sibling"
			basic_die(dieset& ds, shared_ptr<basic_die> p_d);
			
			// "first child"
			basic_die(shared_ptr<basic_die> p_d);
			 
			// "specific offset"
			basic_die(dieset& ds, Dwarf_Off off);
			
		    Dwarf_Off get_offset() const;
            Dwarf_Half get_tag() const;
            boost::shared_ptr<spec::basic_die> get_parent();
            boost::shared_ptr<spec::basic_die> get_first_child();
            Dwarf_Off get_first_child_offset() const;
            boost::shared_ptr<spec::basic_die> get_next_sibling();
            Dwarf_Off get_next_sibling_offset() const;
            opt<std::string> get_name() const;
            const spec::abstract_def& get_spec() const;
            abstract_dieset& get_ds();
            const abstract_dieset& get_ds() const;
            virtual std::map<Dwarf_Half, encap::attribute_value> get_attrs(); 
            // ^^^ not a const function, because may create backrefs
		};
        
//         
//         struct type_die : public lib::basic_die, public virtual spec::type_die
//         {
//         	Dwarf_Unsigned get_size() const;
//             Dwarf_Unsigned size() const { return get_size(); }
//         }
//         
//         struct with_static_location_die 
//         : public lib::basic_die, public virtual spec::with_static_location_die
//         {
//         	encap::loclist get_static_location() const;
//             encap::loclist static_location() const { return get_static_location(); };
//         };

		struct compile_unit_die; // forward decl
		struct file_toplevel_die : public lib::basic_die, public virtual spec::file_toplevel_die
		{
			int prev_version_stamp;
			const spec::abstract_def *p_spec;
			struct cu_info_t
			{
				int version_stamp;
				Dwarf_Half address_size;
				shared_ptr<lib::srcfiles> source_files;
			};
			std::map<Dwarf_Off, cu_info_t> cu_info;
			file_toplevel_die(dieset& ds)
			 : basic_die(ds), prev_version_stamp(-1), p_spec(0) {}
			Dwarf_Off get_offset() const { return 0UL; }
			Dwarf_Half get_tag() const { return 0UL; }
			boost::shared_ptr<spec::basic_die> get_parent() { return boost::shared_ptr<spec::basic_die>(); }
			boost::shared_ptr<spec::basic_die> get_first_child(); 
			Dwarf_Off get_first_child_offset() const;
			Dwarf_Off get_next_sibling_offset() const;
			opt<std::string> get_name() const { return 0; }
			const spec::abstract_def& get_spec() const { assert(p_spec); return *p_spec; }
			
			/* Getters for per-CU state */
			Dwarf_Half get_address_size_for_cu(shared_ptr<compile_unit_die> cu) const;
			std::string source_file_name_for_cu(shared_ptr<compile_unit_die> cu,
				unsigned o);
			unsigned source_file_count_for_cu(shared_ptr<compile_unit_die> cu);
			
			// toplevel DIE has no attrs
			std::map<Dwarf_Half, encap::attribute_value> get_attrs()
			{ return std::map<Dwarf_Half, encap::attribute_value>(); }
			
			// helper
			friend void add_cu_info(void *arg, 
				Dwarf_Off off,
				Dwarf_Unsigned cu_header_length,
				Dwarf_Half version_stamp,
				Dwarf_Unsigned abbrev_offset,
				Dwarf_Half address_size,
				Dwarf_Unsigned next_cu_header);
		private:
			// the actual member function
			void add_cu_info(Dwarf_Off off,
				Dwarf_Unsigned cu_header_length,
				Dwarf_Half version_stamp,
				Dwarf_Unsigned abbrev_offset,
				Dwarf_Half address_size,
				Dwarf_Unsigned next_cu_header);
		};
		// helper function for lib callback
		void add_cu_info(void *arg, 
				Dwarf_Off off,
				Dwarf_Unsigned cu_header_length,
				Dwarf_Half version_stamp,
				Dwarf_Unsigned abbrev_offset,
				Dwarf_Half address_size,
				Dwarf_Unsigned next_cu_header);
		
        class dieset : public virtual abstract_dieset // virtual s.t. can derive from std::map
        {                                             // and have its methods satisfy interface
			file *p_f; // optional
            boost::shared_ptr<basic_die> m_toplevel;
            std::map<Dwarf_Off, Dwarf_Off> parent_cache; // HACK: doesn't evict
            friend class basic_die;
            friend class file_toplevel_die;
            friend class compile_unit_die;

			// private "get"
			boost::shared_ptr<basic_die> get(Dwarf_Off off);
			//boost::shared_ptr<basic_die> get(boost::shared_ptr<die> p_d);
			boost::shared_ptr<basic_die> get(const lib::die& d);

			// the usual HACK for make_shared calling private constructors! 
			template<typename T, typename... Args > 
			static boost::shared_ptr<basic_die>
			my_make_shared(Args&&... args) 
			{ boost::shared_ptr<basic_die> p(new T(std::forward<Args>(args)...)); return p; }

		public:

			// construct from a file
			dieset(file& f) : p_f(&f), 
				m_toplevel(boost::make_shared<file_toplevel_die>(*this)) {}
			// construct empty
			//dieset() : p_f(0), 
			//	m_toplevel(boost::make_shared<file_toplevel_die>(*this)) {}

			iterator find(Dwarf_Off off);
			iterator begin();
			iterator end();

			// FIXME: aranges interface was broken because I confused it with ranges
			//encap::arangelist arangelist_at(Dwarf_Unsigned i) const;
			//{ return encap::rangelist(p_f->get_ranges(), i); }

			std::deque< abstract_dieset::position >
			path_from_root(Dwarf_Off off);

			// support associative indexing
			boost::shared_ptr<spec::basic_die> operator[](Dwarf_Off off) const;

			// backlinks aren't necessarily stored, so support search for parent
			boost::shared_ptr<basic_die> find_parent_of(Dwarf_Off off);
			Dwarf_Off find_parent_offset_of(Dwarf_Off off);            

			// get the toplevel die
			boost::shared_ptr<spec::file_toplevel_die> toplevel(); /* NOT const */

			const spec::abstract_def& get_spec() const { return spec::DEFAULT_DWARF_SPEC; } // FIXME
			
			// get the address size
			Dwarf_Half get_address_size() const
			{
				auto nonconst_this = const_cast<dieset *>(this); // HACK
				auto nonconst_toplevel = dynamic_pointer_cast<file_toplevel_die>(
					nonconst_this->m_toplevel);
				assert(nonconst_toplevel->compile_unit_children_begin()
					!= nonconst_toplevel->compile_unit_children_end());
				return nonconst_toplevel->get_address_size_for_cu(
					dynamic_pointer_cast<lib::compile_unit_die>(
						*(nonconst_toplevel->compile_unit_children_begin())));
			}
		};


/****************************************************************/
/* begin generated ADT includes                                 */
/****************************************************************/
#define forward_decl(t) class t ## _die;
//#define declare_base(base) base ## _die
//#define base_fragment(base) base ## _die(ds, p_d) {}
//#define initialize_base(fragment) virtual spec:: fragment ## _die(ds, p_d)
#define constructor(fragment) \
		/* "exactly this" */ \
		friend class dieset; \
		private: fragment ## _die(const lib::die& d, dieset& ds) \
		: basic_die(d, ds) {} \
		/* "next sibling" */ \
		public: fragment ## _die(dieset& ds, shared_ptr<basic_die> p_prevsib) \
		 : basic_die(ds, p_prevsib) {} \
		/* "first child" */ \
		fragment ## _die(shared_ptr<basic_die> p_parent) \
		 : basic_die(p_parent) {} \
		/* "specific offset" */ \
		fragment ## _die(dieset& ds, Dwarf_Off off) \
		 : basic_die(ds, off) {}
#define begin_class(fragment, base_inits, ...) \
	struct fragment ## _die : public lib::basic_die, public virtual spec:: fragment ## _die { \
    	constructor(fragment)
/* #define base_initializations(...) __VA_ARGS__ */
#define end_class(fragment) \
	};

#define stored_type_string std::string
#define stored_type_flag bool
#define stored_type_unsigned Dwarf_Unsigned
#define stored_type_signed Dwarf_Signed
#define stored_type_offset Dwarf_Off
#define stored_type_half Dwarf_Half
#define stored_type_ref Dwarf_Off
#define stored_type_tag Dwarf_Half
#define stored_type_loclist dwarf::encap::loclist
#define stored_type_address dwarf::encap::attribute_value::address
#define stored_type_refdie boost::shared_ptr<spec::basic_die> 
#define stored_type_refdie_is_type boost::shared_ptr<spec::type_die> 
#define stored_type_rangelist dwarf::encap::rangelist

#define attr_optional(name, stored_t) \
	opt<stored_type_ ## stored_t> get_ ## name() const \
    { Dwarf_Bool has; if (this->hasattr(DW_AT_ ## name, &has), has) { \
    attribute_array arr(*const_cast<lib::die*>(static_cast<const lib::die*>(this))); \
    attribute a = arr[DW_AT_ ## name]; \
    return encap::attribute_value(*this->p_ds, a).get_ ## stored_t (); } \
    else return opt<stored_type_ ## stored_t>(); }

#define super_attr_optional(name, stored_t) attr_optional(name, stored_t)

#define attr_mandatory(name, stored_t) \
	stored_type_ ## stored_t get_ ## name() const \
    { Dwarf_Bool has; assert ((this->hasattr(DW_AT_ ## name, &has), has)); \
    attribute_array arr(*const_cast<lib::die*>(static_cast<const lib::die*>(this)));\
    attribute a = arr[DW_AT_ ## name]; \
    return encap::attribute_value(*this->p_ds, a).get_ ## stored_t (); \
    }

#define super_attr_mandatory(name, stored_t) attr_mandatory(name, stored_t)
// NOTE: super_attr distinction is necessary because
// we do inherit from virtual DIEs in the abstract (spec) realm
// -- so we don't need to repeat definitions of attribute accessor functions there
// we *don't* inherit from virtual DIEs in the concrete realm
// -- so we *do* need to repeat definitions of these attribute accessor functions

#define child_tag(arg) 

// compile_unit_die has an override for get_next_sibling()
#define extra_decls_compile_unit \
		boost::shared_ptr<spec::basic_die> get_next_sibling(); \
		Dwarf_Off get_next_sibling_offset() const; \
		Dwarf_Half get_address_size() const; \
		std::string source_file_name(unsigned o) const; \
		unsigned source_file_count() const;

#include "dwarf3-adt.h"

#undef extra_decls_subprogram
#undef extra_decls_compile_unit

#undef forward_decl
#undef declare_base
#undef base_fragment
#undef initialize_base
#undef constructor
#undef begin_class
#undef base_initializations
#undef end_class
#undef stored_type_string
#undef stored_type_flag
#undef stored_type_unsigned
#undef stored_type_signed
#undef stored_type_offset
#undef stored_type_half
#undef stored_type_ref
#undef stored_type_tag
#undef stored_type_loclist
#undef stored_type_address
#undef stored_type_refdie
#undef stored_type_refdie_is_type
#undef stored_type_rangelist
#undef attr_optional
#undef attr_mandatory
#undef super_attr_optional
#undef super_attr_mandatory
#undef child_tag

/****************************************************************/
/* end generated ADT includes                                   */
/****************************************************************/

    }
}

#endif
