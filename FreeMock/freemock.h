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
		void restore_region_protection() const noexcept {
			DWORD dwDummy = 0;
			VirtualProtect(f1_, 8, dwOldProtect_, &dwDummy);
		}

		void restore_function_memory() const noexcept {
			*reinterpret_cast<unsigned*>(f1_) = dwOriginalRegionBytes_[0];
			*(reinterpret_cast<unsigned*>(f1_) + 1) = dwOriginalRegionBytes_[1];
		}

		void handle_jump_table() const noexcept {
			if (*reinterpret_cast<unsigned char*>(f1_) == 0xe9) {
				auto relativeJump = *reinterpret_cast<unsigned*>(reinterpret_cast<unsigned char*>(f1_) + 1);
				auto finalAddress = reinterpret_cast<int>(f1_) + relativeJump + 5;
				f1_ = *reinterpret_cast<Func1*>(&finalAddress);
			}
		}

		void add_write_region_protection() const {
			if (!VirtualProtect(f1_, 8, dwNewProtect_, &dwOldProtect_)) {
				throw std::runtime_error{ "Failed to change region protection" };
			}
		}

		void save_old_function_memory() const noexcept {
			dwOriginalRegionBytes_[0] = *reinterpret_cast<unsigned*>(f1_);
			dwOriginalRegionBytes_[1] = *(reinterpret_cast<unsigned*>(f1_) + 1);
		}

		void patch_memory_region() const noexcept {
			// Mov eax, funcptr
			*(reinterpret_cast<unsigned char*>(f1_)) = 0xb8;
			*(reinterpret_cast<unsigned*>(reinterpret_cast<unsigned char*>(f1_) + 1)) = *reinterpret_cast<unsigned*>(f2_);
			// JMP eax
			*(reinterpret_cast<unsigned short*>(reinterpret_cast<unsigned char*>(f1_) + 5)) = 0xe0ff;
		}
	private:
		mutable Func1 f1_;
		Func2 f2_;
		mutable DWORD dwOldProtect_ = PAGE_EXECUTE_READ;
		mutable DWORD dwNewProtect_ = PAGE_EXECUTE_READWRITE;
		mutable DWORD dwOriginalRegionBytes_[2] = { 0 };
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