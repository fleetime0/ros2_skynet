#ifndef SUBJECT_H_
#define SUBJECT_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

template<typename T>
class Observable {
public:
  Observable() = default;
  ~Observable() = default;
  using OnMessageFunc = std::function<void(T)>;

  void Subscribe(OnMessageFunc func) { subscribed_func_ = std::move(func); }
  void UnSubscribe() { subscribed_func_ = nullptr; }

  OnMessageFunc subscribed_func_;
};

template<typename T>
class Subject {
public:
  virtual ~Subject() = default;
  virtual void Next(T message) {
    auto it = observers_.begin();
    while (it != observers_.end()) {
      if (auto obs = it->lock()) {
        if (obs->subscribed_func_)
          obs->subscribed_func_(message);
        ++it;
      } else {
        it = observers_.erase(it);
      }
    }
  }

  virtual std::shared_ptr<Observable<T>> AsObservable() {
    auto observer = std::make_shared<Observable<T>>();
    observers_.push_back(observer);
    return observer;
  }

  virtual void UnSubscribe() { observers_.clear(); }

protected:
  std::vector<std::weak_ptr<Observable<T>>> observers_;

  void RemoveNullObservers() {
    auto new_end = std::remove_if(observers_.begin(), observers_.end(),
                                  [](const std::weak_ptr<Observable<T>> &w) { return w.expired(); });
    observers_.erase(new_end, observers_.end());
  }
};

#endif
