#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>

namespace freemock {
	template <typename Func1, typename Func2>
	struct FreeMocker {
		FreeMocker(Func1 f1, Func2 f2) : f1_{ f1 }, f2_{ f2 } {
			mock();
		}
		~FreeMocker() noexcept {
			unmock();
		}

		void mock() const {
			if (!mocked) {
				handle_jump_table();
				add_write_region_protection();
				save_old_function_memory();
				patch_memory_region();
				mocked = true;
			}
		}

		void unmock() const noexcept {
			if (mocked) {
				restore_function_memory();
				restore_region_protection();
				mocked = false;
			}
		}


	private:
		static inline void* get_address_offsetted(void* const ptr, const size_t offset) {
			return (reinterpret_cast<unsigned char*>(ptr) + offset);
		}

		template<class T>
		static inline T get_value_from_address(void* const ptr) {
			return *reinterpret_cast<T*>(ptr);
		}

		template<class T>
		static inline void set_value_to_address(void* const ptr, const T value) {
			*(reinterpret_cast<T*>(ptr)) = value;
		}

		template<class T>
		static inline T get_value_from_address_offsetted(void* const ptr, const size_t offset) {
			return get_value_from_address<T>(get_address_offsetted(ptr, offset));
		}

		void restore_region_protection() const noexcept {
			DWORD dwDummy = 0;
			VirtualProtect(f1_, sizeof(dwOriginalRegionBytes_), dwOldProtect_, &dwDummy);
		}

		void restore_function_memory() const noexcept {
			for (auto i = 0u; i < DWORDBlocksToSave; ++i)
			{
			*(reinterpret_cast<unsigned*>(f1_) + i) = dwOriginalRegionBytes_[i];
			}
		}

		void handle_jump_table() const noexcept {
			static const unsigned char RelativeJumpIndicator = 0xe9;
			if (get_value_from_address<unsigned char>(f1_) == RelativeJumpIndicator) {
				auto relativeJump = get_value_from_address_offsetted<unsigned>(f1_, 1);
				auto finalAddress = get_address_offsetted(f1_, relativeJump + 5);
				f1_ = get_value_from_address<Func1>(&finalAddress);
			}
		}

		void add_write_region_protection() const {
			if (!VirtualProtect(f1_, sizeof(dwOriginalRegionBytes_), dwNewProtect_, &dwOldProtect_)) {
				throw std::runtime_error{ "Failed to change region protection" };
			}
		}

		void save_old_function_memory() const noexcept {
			for (auto i = 0u; i < DWORDBlocksToSave; ++i)
			{
			dwOriginalRegionBytes_[i] = get_value_from_address_offsetted<DWORD>(f1_, sizeof(DWORD)*i);
			}
		}

		void patch_memory_region() const noexcept {
			// x86: mov eax, funcptr
			// x64: mov rax, funcptr
#ifdef _WIN64
			uint16_t movOpcode = 0xb848;
#else
			uint8_t movOpcode = 0xb8;
#endif
			set_value_to_address(f1_, movOpcode);
			set_value_to_address(get_address_offsetted(f1_, sizeof(movOpcode)), get_value_from_address<uintptr_t>(f2_));
			const size_t TotalSizeMov = sizeof(movOpcode) + sizeof(uintptr_t); // opcode + address
			// x86: jmp eax
			// x64: jmp rax
			const uint16_t jmpOpcode = 0xe0ff;
			set_value_to_address(get_address_offsetted(f1_, TotalSizeMov), jmpOpcode);
		}
	private:
		mutable Func1 f1_;
		Func2 f2_;
		mutable DWORD dwOldProtect_ = PAGE_EXECUTE_READ;
		mutable DWORD dwNewProtect_ = PAGE_EXECUTE_READWRITE;

#ifdef _WIN64
		static const unsigned int DWORDBlocksToSave = 3u;
#else
		static const unsigned int DWORDBlocksToSave = 2u;
#endif
		mutable DWORD dwOriginalRegionBytes_[DWORDBlocksToSave] = { 0 };
		mutable bool mocked = false;
	};

	template <typename Func1, typename Func2>
	auto make_mock(Func1 f1, Func2 f2, std::enable_if_t<std::is_function<std::remove_pointer_t<Func2>>::value, void*> = 0) {
		return FreeMocker<Func1, decltype(&f2)>(f1, &f2);
	}

	namespace traits {
		template <class T>
		struct LambdaWrapper : public LambdaWrapper<decltype(&T::operator())> {};

		template <typename Ret, typename Class, typename... Args>
		struct LambdaWrapper<Ret(Class::*)(Args...) const> {
			using funcPtr = Ret(__cdecl *)(Args...);
		};

		template <typename Ret, typename Class, typename... Args>
		struct LambdaWrapper<Ret(Class::*)(Args...)> {
			using funcPtr = Ret(__cdecl *)(Args...);
		};
	}

	template <typename Func1, typename Func2>
	auto make_mock(Func1 f1, Func2 f2, std::enable_if_t<!std::is_function<std::remove_pointer_t<Func2>>::value, void*> = 0) {
		traits::LambdaWrapper<Func2>::funcPtr wrapper = f2;
		return FreeMocker<Func1, decltype(&wrapper)>(f1, &wrapper);
	}
}
