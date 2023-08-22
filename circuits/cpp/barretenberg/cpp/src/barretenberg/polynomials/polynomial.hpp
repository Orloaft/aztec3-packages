#pragma once
#include "barretenberg/common/mem.hpp"
#include "barretenberg/crypto/sha256/sha256.hpp"
#include "barretenberg/ecc/curves/grumpkin/grumpkin.hpp"
#include "evaluation_domain.hpp"
#include "polynomial_arithmetic.hpp"
#include <concepts>
#include <cstddef>
#include <fstream>
#include <span>
namespace barretenberg {
template <typename Fr> class Polynomial {
  public:
    /**
     * Implements requirements of `std::ranges::contiguous_range` and `std::ranges::sized_range`
     */
    using value_type = Fr;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using pointer = std::shared_ptr<value_type[]>;
    using const_pointer = pointer;
    using iterator = Fr*;
    using const_iterator = Fr const*;
    using FF = Fr;

    Polynomial(const size_t initial_size);
    Polynomial(const Polynomial& other);
    Polynomial(const Polynomial& other, const size_t target_size);

    Polynomial(Polynomial&& other) noexcept;

    // Create a polynomial from the given fields.
    Polynomial(std::span<const Fr> coefficients);

    // Allow polynomials to be entirely reset/dormant
    Polynomial() = default;

    /**
     * @brief Create the degree-(m-1) polynomial T(X) that interpolates the given evaluations.
     * We have T(xⱼ) = yⱼ for j=1,...,m
     *
     * @param interpolation_points (x₁,…,xₘ)
     * @param evaluations (y₁,…,yₘ)
     */
    Polynomial(std::span<const Fr> interpolation_points, std::span<const Fr> evaluations);

    Polynomial& operator=(Polynomial&& other) noexcept;
    Polynomial& operator=(const Polynomial& other);
    ~Polynomial();

    /**
     * Return a shallow clone of the polynomial. i.e. underlying memory is shared.
     */
    Polynomial clone()
    {
        Polynomial p;
        p.coefficients_ = coefficients_;
        p.size_ = size_;
        return p;
    }

    std::array<uint8_t, 32> hash() const { return sha256::sha256(byte_span()); }

    void clear()
    {
        coefficients_.reset();
        size_ = 0;
    }

    bool operator==(Polynomial const& rhs) const
    {
        // If either is empty, both must be
        if (is_empty() || rhs.is_empty()) {
            return is_empty() && rhs.is_empty();
        }
        // Size must agree
        if (size() != rhs.size()) {
            return false;
        }
        // Each coefficient must agree
        for (size_t i = 0; i < size(); i++) {
            if (coefficients_.get()[i] != rhs.coefficients_.get()[i]) {
                return false;
            }
        }
        return true;
    }

    // Const and non const versions of coefficient accessors
    Fr const& operator[](const size_t i) const { return coefficients_.get()[i]; }

    Fr& operator[](const size_t i) { return coefficients_.get()[i]; }

    Fr const& at(const size_t i) const
    {
        ASSERT(i < capacity());
        return coefficients_.get()[i];
    };

    Fr& at(const size_t i)
    {
        ASSERT(i < capacity());
        return coefficients_.get()[i];
    };

    Fr evaluate(const Fr& z, const size_t target_size) const;
    Fr evaluate(const Fr& z) const;

    Fr compute_barycentric_evaluation(const Fr& z, const EvaluationDomain<Fr>& domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    Fr evaluate_from_fft(const EvaluationDomain<Fr>& large_domain,
                         const Fr& z,
                         const EvaluationDomain<Fr>& small_domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void fft(const EvaluationDomain<Fr>& domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void partial_fft(const EvaluationDomain<Fr>& domain, Fr constant = 1, bool is_coset = false)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void coset_fft(const EvaluationDomain<Fr>& domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void coset_fft(const EvaluationDomain<Fr>& domain,
                   const EvaluationDomain<Fr>& large_domain,
                   const size_t domain_extension)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void coset_fft_with_constant(const EvaluationDomain<Fr>& domain, const Fr& costant)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void coset_fft_with_generator_shift(const EvaluationDomain<Fr>& domain, const Fr& constant)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void ifft(const EvaluationDomain<Fr>& domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void ifft_with_constant(const EvaluationDomain<Fr>& domain, const Fr& constant)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    void coset_ifft(const EvaluationDomain<Fr>& domain)
        requires polynomial_arithmetic::SupportsFFT<Fr>;
    Fr compute_kate_opening_coefficients(const Fr& z)
        requires polynomial_arithmetic::SupportsFFT<Fr>;

    bool is_empty() const { return (coefficients_ == nullptr) || (size_ == 0); }

    /**
     * @brief Returns an std::span of the left-shift of self.
     *
     * @details If the n coefficients of self are (0, a₁, …, aₙ₋₁),
     * we returns the view of the n-1 coefficients (a₁, …, aₙ₋₁).
     */
    std::span<Fr> shifted() const
    {
        ASSERT(size_ > 0);
        ASSERT(coefficients_[0].is_zero());
        ASSERT(coefficients_.get()[size_].is_zero()); // relies on DEFAULT_CAPACITY_INCREASE >= 1
        return std::span{ coefficients_.get() + 1, size_ };
    }

    /**
     * @brief adds the polynomial q(X) 'other', multiplied by a scaling factor.
     *
     * @param other q(X)
     * @param scaling_factor scaling factor by which all coefficients of q(X) are multiplied
     */
    void add_scaled(std::span<const Fr> other, Fr scaling_factor);

    /**
     * @brief adds the polynomial q(X) 'other'.
     *
     * @param other q(X)
     */
    Polynomial& operator+=(std::span<const Fr> other);

    /**
     * @brief subtracts the polynomial q(X) 'other'.
     *
     * @param other q(X)
     */
    Polynomial& operator-=(std::span<const Fr> other);

    /**
     * @brief sets this = p(X) to s⋅p(X)
     *
     * @param scaling_factor s
     */
    Polynomial& operator*=(const Fr scaling_facor);

    /**
     * @brief evaluates p(X) = ∑ᵢ aᵢ⋅Xⁱ considered as multi-linear extension p(X₀,…,Xₘ₋₁) = ∑ᵢ aᵢ⋅Lᵢ(X₀,…,Xₘ₋₁)
     * at u = (u₀,…,uₘ₋₁)
     *
     * @details this function allocates a temporary buffer of size n/2
     *
     * @param evaluation_points an MLE evaluation point u = (u₀,…,uₘ₋₁)
     * @param shift evaluates p'(X₀,…,Xₘ₋₁) = 1⋅L₀(X₀,…,Xₘ₋₁) + ∑ᵢ˲₁ aᵢ₋₁⋅Lᵢ(X₀,…,Xₘ₋₁) if true
     * @return Fr p(u₀,…,uₘ₋₁)
     */
    Fr evaluate_mle(std::span<const Fr> evaluation_points, bool shift = false) const;

    /**
     * @brief Divides p(X) by (X-r₁)⋯(X−rₘ) in-place.
     * Assumes that p(rⱼ)=0 for all j
     *
     * @details we specialize the method when only a single root is given.
     * if one of the roots is 0, then we first factor all other roots.
     * dividing by X requires only a left shift of all coefficient.
     *
     * @param roots list of roots (r₁,…,rₘ)
     */
    void factor_roots(std::span<const Fr> roots) { polynomial_arithmetic::factor_roots(std::span{ *this }, roots); };
    void factor_roots(const Fr& root) { polynomial_arithmetic::factor_roots(std::span{ *this }, root); };

#ifdef __clang__
    // Needed for clang versions earlier than 14.0.3, but breaks gcc.
    // Can remove once ecosystem is firmly upgraded.
    operator std::span<Fr>() { return std::span<Fr>(coefficients_.get(), size_); }
    operator std::span<const Fr>() const { return std::span<const Fr>(coefficients_.get(), size_); }
#endif

    iterator begin() { return coefficients_.get(); }
    iterator end() { return coefficients_.get() + size_; }
    pointer data() { return coefficients_; }

    std::span<uint8_t> byte_span() const
    {
        return std::span<uint8_t>((uint8_t*)coefficients_.get(), size_ * sizeof(fr));
    }

    const_iterator begin() const { return coefficients_.get(); }
    const_iterator end() const { return coefficients_.get() + size_; }
    const_pointer data() const { return coefficients_; }

    std::size_t size() const { return size_; }
    std::size_t capacity() const { return size_ + DEFAULT_CAPACITY_INCREASE; }

  private:
    // safety check for in place operations
    bool in_place_operation_viable(size_t domain_size = 0) { return (size() >= domain_size); }

    pointer allocate_aligned_memory(const size_t size) const;

    void zero_memory_beyond(const size_t start_position);
    // When a polynomial is instantiated from a size alone, the memory allocated corresponds to
    // input size + DEFAULT_CAPACITY_INCREASE. A DEFAULT_CAPACITY_INCREASE of >= 1 is required to ensure
    // that polynomials can be 'shifted' via a span of the 1st to size+1th coefficients.
    const static size_t DEFAULT_CAPACITY_INCREASE = 1;

    pointer coefficients_;
    // The size_ effectively represents the 'usable' length of the coefficients array but may be less than the true
    // 'capacity' of the array. It is not explicitly tied to the degree and is not changed by any operations on the
    // polynomial.
    size_t size_ = 0;
};

template <typename Fr> inline std::ostream& operator<<(std::ostream& os, Polynomial<Fr> const& p)
{
    return os << "[ data\n"
              << "  " << p[0] << ",\n"
              << "  " << p[1] << ",\n"
              << "  ... ,\n"
              << "  " << p[p.size() - 2] << ",\n"
              << "  " << p[p.size() - 1] << ",\n"
              << "]";
}

// Done
// N.B. grumpkin polynomials don't support fast fourier transforms using roots of unity!
// TODO: use template junk to disable fft methods if Fr::SUPPORTS_FFTS == false
// extern template class Polynomial<grumpkin::fr>;
extern template class Polynomial<barretenberg::fr>;
extern template class Polynomial<grumpkin::fr>;

using polynomial = Polynomial<barretenberg::fr>;

} // namespace barretenberg

/**
 * The static_assert below ensure that that our Polynomial class correctly models an `std::ranges::contiguous_range`,
 * and other requirements that allow us to convert a `Polynomial<Fr>` to a `std::span<const Fr>`.
 *
 * This also means we can now iterate over the elements in the vector using a `for(auto ...)` loop, and use various std
 * algorithms.
 *
 * static_assert(std::ranges::contiguous_range<barretenberg::polynomial>);
 * static_assert(std::ranges::sized_range<barretenberg::polynomial>);
 * static_assert(std::convertible_to<barretenberg::polynomial, std::span<const barretenberg::fr>>);
 * static_assert(std::convertible_to<barretenberg::polynomial&, std::span<barretenberg::fr>>);
 * // cannot convert a const polynomial to a non-const span
 * static_assert(!std::convertible_to<const barretenberg::polynomial&, std::span<barretenberg::fr>>);
 * static_assert(std::convertible_to<const barretenberg::polynomial&, std::span<const barretenberg::fr>>);
 */