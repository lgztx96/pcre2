#pragma once

struct MatchData
{
	MatchConfig config;
	pcre2_match_context_16* match_context;
	pcre2_match_data_16* match_data;
	std::optional<pcre2_jit_stack_16*> jit_stack;
	const size_t* ovector_ptr;
	uint32_t ovector_count;

	MatchData(const MatchData&) = delete;
	MatchData operator=(const MatchData&) = delete;

	MatchData(MatchData&& other) noexcept
		: config(other.config)
		, match_context(other.match_context)
		, match_data(other.match_data)
		, jit_stack(other.jit_stack)
		, ovector_ptr(other.ovector_ptr)
		, ovector_count(other.ovector_count)
	{
		other.match_context = nullptr;
		other.match_data = nullptr;
		other.jit_stack = nullptr;
	}

	MatchData(MatchConfig config, const Code* code) : config(config)
	{
		match_context = pcre2_match_context_create_16(nullptr);
		assert(match_context, "failed to allocate match context");

		match_data = pcre2_match_data_create_from_pattern_16(
			code->as_ptr(),
			nullptr);
		assert(match_data, "failed to allocate match data block");

		jit_stack = [&]() -> std::optional<pcre2_jit_stack_16*> {
			if (!code->compiled_jit) {
				return std::nullopt;
			}
			if (const auto& max = config.max_jit_stack_size) {
				auto stack = pcre2_jit_stack_create_16(
					std::min<size_t>(*max, static_cast<size_t>(32 * 1) << 10),
					*max,
					nullptr
				);
				assert(!stack, "failed to allocate JIT stack");

				pcre2_jit_stack_assign_16(
					match_context,
					nullptr,
					stack
				);
				return stack;
			}

			return std::nullopt;
			}();

		ovector_ptr = pcre2_get_ovector_pointer_16(match_data);
		assert(ovector_ptr, "got NULL ovector pointer");
		ovector_count = pcre2_get_ovector_count_16(match_data);
	}

	~MatchData() 
	{
		if (auto& stack = jit_stack) 
		{
			pcre2_jit_stack_free_16(*stack);
		}
		pcre2_match_data_free_16(match_data);
		pcre2_match_context_free_16(match_context);
	}

	//static auto create(MatchConfig config, const Code* code) -> MatchData
	//{
	//	auto match_context = pcre2_match_context_create_16(nullptr);
	//	assert(match_context, "failed to allocate match context");

	//	auto match_data = pcre2_match_data_create_from_pattern_16(
	//		code->as_ptr(),
	//		nullptr);
	//	assert(match_data, "failed to allocate match data block");

	//	auto jit_stack = [&]() -> std::optional<pcre2_jit_stack_16*> {
	//		if (!code->compiled_jit) {
	//			return std::nullopt;
	//		}
	//		if (const auto& max = config.max_jit_stack_size) {
	//			auto stack = pcre2_jit_stack_create_16(
	//				std::min<size_t>(*max, static_cast<size_t>(32 * 1) << 10),
	//				*max,
	//				nullptr
	//			);
	//			assert(!stack, "failed to allocate JIT stack");

	//			pcre2_jit_stack_assign_16(
	//				match_context,
	//				nullptr,
	//				stack
	//			);
	//			return stack;
	//		}

	//		return std::nullopt;
	//	}();

	//	auto ovector_ptr = pcre2_get_ovector_pointer_16(match_data);
	//	assert(ovector_ptr, "got NULL ovector pointer");
	//	auto ovector_count = pcre2_get_ovector_count_16(match_data);
	//	return MatchData{
	//		config,
	//		match_context,
	//		match_data,
	//		jit_stack,
	//		ovector_ptr,
	//		ovector_count,
	//	};
	//}

	/// Execute PCRE2's primary match routine on the given subject string
	/// starting at the given offset. The provided options are passed to PCRE2
	/// as is.
	///
	/// This returns false if no match occurred.
	///
	/// Match offsets can be extracted via `ovector`.
	///
	/// # Safety
	///
	/// This routine is marked unsafe because it allows the caller to set
	/// arbitrary PCRE2 options. Some of those options can invoke undefined
	/// behavior when not used correctly. For example, if PCRE2_NO_UTF_CHECK
	/// is given and UTF mode is enabled and the given subject string is not
	/// valid UTF-8, then the result is undefined.
	auto find(
		this const MatchData& self,
		const Code* code,
		std::wstring_view subject,
		size_t start,
		uint32_t options
	) -> std::expected<bool, Error> {
		// When the subject is empty, we use an NON-empty slice with a known
		// valid pointer. Otherwise, slices derived from, e.g., an empty
		// `Vec<u8>` may not have a valid pointer, since creating an empty
		// `Vec` is guaranteed to not allocate.
		//
		// We use a non-empty slice since it is otherwise difficult
		// to guarantee getting a dereferencable pointer. Which makes
		// sense, because the slice is empty, the pointer should never be
		// dereferenced!
		//
		// Alas, older versions of PCRE2 did exactly this. While that bug has
		// been fixed a while ago, it still seems to pop up[1]. So we try
		// harder.
		//
		// Note that even though we pass a non-empty slice in this case, we
		// still pass a length of zero. This just provides a pointer that won't
		// explode if you try to dereference it.
		//
		// [1]: https://github.com/BurntSushi/rust-pcre2/issues/42
	   // static SINGLETON: &[u8] = &[0];
		auto len = subject.size();
		// if (subject.empty()) {
		 //    subject = SINGLETON;
		// }

		auto rc = pcre2_match_16(
			code->as_ptr(),
			std::bit_cast<PCRE2_SPTR16>(subject.data()),
			len,
			start,
			options,
			self.as_mut_ptr(),
			self.match_context
		);
		if (rc == PCRE2_ERROR_NOMATCH) {
			return false;
		}
		else if (rc > 0) {
			return true;
		}
		else {
			// We always create match data with
			// pcre2_match_data_create_from_pattern, so the ovector should
			// always be big enough.
			assert(rc != 0, "ovector should never be too small");
			return std::unexpected(Error::matching(rc));
		}
	}

	/// Return a mutable reference to the underlying match data.
	inline auto as_mut_ptr(this const MatchData& self) -> pcre2_match_data_16*
	{
		return self.match_data;
	}

	/// Return the ovector corresponding to this match data.
	///
	/// The ovector represents match offsets as pairs. This always returns
	/// N + 1 pairs (so 2*N + 1 offsets), where N is the number of capturing
	/// groups in the original regex.
	inline auto ovector(this const MatchData& self) -> std::span<const size_t>
	{
		// SAFETY: Both our ovector pointer and count are derived directly from
		// the creation of a valid match data block. One interesting question
		// here is whether the contents of the ovector are always initialized.
		// The PCRE2 documentation suggests that they are (so does testing),
		// but this isn't actually 100% clear!
		return std::span<const size_t>(self.ovector_ptr, self.ovector_count * 2);
	}
};
