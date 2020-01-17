#include <cstdio>
#include <cstdint>
#include <vector>
#include <numeric>

//#include <signal.h> // for debugging

#include <coat/Function.h>
#include <coat/ControlFlow.h>
#include <coat/Vector.h>


// function signature
using func_type = void (*)(const uint32_t*, const uint32_t*, uint32_t*, size_t);

void mean(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
	for(size_t i=0; i<size; ++i){
		r[i] = (a[i] + b[i]) / 2;
	}
}


void mean_coat(
	const uint32_t * __restrict__ a,
	const uint32_t * __restrict__ b,
	      uint32_t * __restrict__ r,
	size_t size
){
#ifdef ENABLE_ASMJIT
	// init
	coat::runtimeasmjit asmrt;
	// context object
	coat::Function<coat::runtimeasmjit,func_type> fn(asmrt, "gen_asmjit");
#elif defined(ENABLE_LLVMJIT)
	// init
	coat::runtimellvmjit::initTarget();
	coat::runtimellvmjit llvmrt;
	llvmrt.setOptLevel(2);
	// context object
	coat::Function<coat::runtimellvmjit,func_type> fn(llvmrt, "gen_llvmjit");
#else
#	error "Neither AsmJit nor LLVM enabled"
#endif
	{
		auto [aptr,bptr,rptr,sze] = fn.getArguments("a", "b", "r", "size");
		coat::Value pos(fn, uint64_t(0), "pos");

		coat::do_while(fn, [&]{
			// rptr[pos] = (make_vector<8>(fn, aptr[pos]) += bptr[pos]) /= 2;
			auto vec = coat::make_vector<8>(fn, aptr[pos]);
			vec += bptr[pos];
			vec /= 2;
			vec.store(rptr[pos]);

			pos += vec.getWidth();
		}, pos <= sze);
		//TODO: tail handling
		coat::ret(fn);
	}
#ifdef ENABLE_LLVMJIT
	if(!llvmrt.verifyFunctions()){
		puts("verification failed. aborting.");
		llvmrt.print("failed.ll");
		exit(EXIT_FAILURE);
	}
	llvmrt.optimize();
	if(!llvmrt.verifyFunctions()){
		puts("verification after optimization failed. aborting.");
		llvmrt.print("failed_opt.ll");
		exit(EXIT_FAILURE);
	}
#endif
	func_type foo = fn.finalize();

	// execute
	//raise(SIGTRAP); // stop debugger here
	foo(a, b, r, size);
}


static void print(const std::vector<uint32_t> &vec){
	for(size_t i=0, s=vec.size(); i<s; ++i){
		printf("%u, ", vec[i]);
	}
	printf("\n");
}

int main(){
	// generate some data
	std::vector<uint32_t> a, b, r, e;
	static const int datasize = 4*1024*1024;
	a.resize(datasize);
	std::iota(a.begin(), a.end(), 0);
	b.resize(datasize);
	std::iota(b.begin(), b.end(), 2);
	r.resize(datasize);
	e.resize(datasize);
	std::iota(e.begin(), e.end(), 1);

	// call
	mean(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean: Success");
	}else{
		puts("mean: Failure");
		print(r);
	}

	// coat
	mean_coat(a.data(), b.data(), r.data(), r.size());
	if(r == e){
		puts("mean_coat: Success");
		return 0;
	}else{
		puts("mean_coat: Failure");
		print(r);
		return -1;
	}
}
