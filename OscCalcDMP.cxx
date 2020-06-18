#include "OscCalcDMP.h"
#include <cmath>

#include "PMNS_DMP.h"

#ifndef DARWINBUILD
#include "Utilities/Stan.h"
#endif

namespace osc
{

  template<typename T>
  void _OscCalcDMP<T>::SaveLastParams()
  {
    this->fLastParams.L = this->fL;
    this->fLastParams.rho = this->fRho;
    this->fLastParams.dmsq21 = this->fDmsq21;
    this->fLastParams.dmsq32 = this->fDmsq32;
    this->fLastParams.th12 = this->fTh12;
    this->fLastParams.th13 = this->fTh13;
    this->fLastParams.th23 = this->fTh23;
    this->fLastParams.deltacp = this->fdCP;
  }

  inline void _sincos(double theta, double &s, double &c)
  {
#ifdef __APPLE_CC__
    __sincos(theta, &s, &c);
#else
    sincos(theta, &s, &c);
#endif
  }

  template<typename T>
  _IOscCalcAdjustable<T> *
  _OscCalcDMP<T>::Copy() const
  {
    return new _OscCalcDMP<T>(*this);
  }

  template<typename T>
  bool _OscCalcDMP<T>::ParamsAreCached()
  {
    return this->fDmsq21 == this->fCache.parameters.dmsq21 &&
    this->fDmsq32 == this->fCache.parameters.dmsq32 &&
    this->fTh12 == this->fCache.parameters.th12 &&
    this->fTh13 == this->fCache.parameters.th13 &&
    this->fTh23 == this->fCache.parameters.th23 &&
    this->fdCP == this->fCache.parameters.deltacp &&
    this->fL == this->fCache.parameters.L &&
    this->fRho == this->fCache.parameters.rho;
  }

  template<typename T>
  inline int _OscCalcDMP<T>::ChannelCacheIdx(int flavBefore, int flavAfter) const
  {
    // rows in the cache are arranged in the following order
    // 11 21 31 12 22 32 13 23 33 -11 -21 -31 -12 -22 -32 -13 -23 -33
    // where nue   = 1
    //       numu  = 2
    //       nutau = 3
    // and negative is for antineutrino
    int anti = flavBefore / abs(flavBefore);
    int i = (abs(flavBefore) - 12) / 2;
    int j = (abs(flavAfter) - 12) / 2;
    int idx = (1 - anti) / 2 * 9 + (3 * j + i);
    return idx;
  }


  template<typename T>
  Matrix<T, Dynamic, 1> _OscCalcDMP<T>::P(int flavBefore, int flavAfter, const std::vector<double> &E)
  {
    if (fCache.energies.size() != (size_t) fCache.probabilities.cols() &&
        fCache.energies.size() != 0)
    { // does a cache exist
      if (ParamsAreCached())
      {
        if (this->fCache.energies == E)
          return this->fCache.probabilities.col(ChannelCacheIdx(flavBefore, flavAfter));
      }
    }
    FillCache(E);
    return this->fCache.probabilities.col(ChannelCacheIdx(flavBefore, flavAfter));
  }

  template<typename T>
  T _OscCalcDMP<T>::P(int flavBefore, int flavAfter, double E,
                                        bool fast_and_loose)
  {
    if (fast_and_loose)
    {
      auto e_it = std::find(fCache.energies.begin(),
                            fCache.energies.end(),
                            E);
      return fCache.probabilities(e_it - fCache.energies.begin(),
                                  ChannelCacheIdx(flavBefore, flavAfter));
    } else
    {
      return P(flavBefore, flavAfter, E);
    }
  }

  template<typename T>
  T _OscCalcDMP<T>::P(int flavBefore, int flavAfter, double E)
  {
    if (!std::is_same<T, stan::math::var>::value &&
        fCache.energies.size() != (size_t) fCache.probabilities.cols() &&
        fCache.energies.size() != 0)
    { // does a cache exist
      if (ParamsAreCached())
      { // do current params match those cached
        auto e_it = std::find(fCache.energies.begin(),
                              fCache.energies.end(),
                              E);
        if (e_it != fCache.energies.end())
        { // is the given energy cached?

          return this->fCache.probabilities(e_it - fCache.energies.begin(),
                                            ChannelCacheIdx(flavBefore, flavAfter));

        }
      }
    }
    // if we get here, the cache is stale.
    // TODO. write function ValidateCache
    this->FillCache({E});
    return this->fCache.probabilities(0, ChannelCacheIdx(flavBefore, flavAfter));
  }


  template<typename T>
  void _OscCalcDMP<T>::FillCache()
  {
    using ArrayXXT = Array<T, Dynamic, Dynamic>;

    const _OscParameters<T> params{this->fDmsq21, this->fDmsq32, this->fTh12, this->fTh13, this->fTh23, this->fdCP,
                                   this->fL, this->fRho};
    _PMNS_DMP<T> OO(this->fCache.ENERGIES, this->fRho, this->fL);
    ArrayXXT _cache = OO.P(params);

    int nbins = this->fCache.energies.size();
    ArrayXXT cache(nbins, 18);
    cache.block(0, 0, nbins, 9) = _cache.block(0, 0, nbins, 9);
    cache.block(0, 9, nbins, 9) = _cache.block(nbins, 0, nbins, 9);

    this->fCache.probabilities = cache;
    this->fCache.parameters = params;
    this->fCache.iter++;
  }

  template<typename T>
  void _OscCalcDMP<T>::FillCache(std::vector<double> const &energies)
  {
    this->SetCachedEnergies(energies);
    this->FillCache();
  }

  template<typename T>
  void _OscCalcDMP<T>::SetCachedEnergies(std::vector<double> const &energies)
  {
    this->fCache.energies = energies;
    int nbins = energies.size();
    ArrayXd EE(2 * nbins);
    EE.segment(0, nbins) = Map<const ArrayXd>(energies.data(), energies.size());
    EE.segment(nbins, nbins) = -1 * Map<const ArrayXd>(energies.data(), energies.size());
    this->fCache.ENERGIES = EE;
  }

//---------------------------------------------------------------------------
  template class osc::_OscCalcDMP<double>;

#ifndef DARWINBUILD
  template class osc::_OscCalcDMP<stan::math::var>;

#endif
}