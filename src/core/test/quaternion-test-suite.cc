/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author
 */

// An essential include is test.h
#include "ns3/test.h"
#include "ns3/quaternion.h"
#include <cmath>
#include <tuple>

/**
 * \file
 * \ingroup core-tests
 * \ingroup testing
 * \ingroup testing-example
 * Quaternion test suite.
 */

/**
 * \ingroup core-tests
 * \defgroup testing-example Example use of TestSuite
 */

namespace ns3 {

namespace tests {


/**
 * \ingroup testing-example
 * This is an example TestCase.
 */
class QuaternionTestCase1 : public TestCase
{
public:
  /** Constructor. */
  QuaternionTestCase1 ();
  /** Destructor. */
  virtual ~QuaternionTestCase1 ();

private:
  virtual void DoRun (void);
};

/** Add some help text to this case to describe what it is intended to test. */
QuaternionTestCase1::QuaternionTestCase1 ()
  : TestCase ("Quaternion test case 1")
{}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
QuaternionTestCase1::~QuaternionTestCase1 ()
{}

/**
 * This method is the pure virtual method from class TestCase that every
 * TestCase must implement
 */
void
QuaternionTestCase1::DoRun (void)
{
  Quaternion Q = Quaternion(M_PI * 0.25, Vector(0, 0, 1));
  Q.normalize();
  std::cout << "Testcase 1" << "\n";
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.GetLength(), 1.0, 0.001, "Not equal within tolerance");
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.angle(), M_PI * 0.25, 0.001, "Not equal within tolerance");
  std::cout << Q.eulerAngles() << "\n";
  std::cout << "Testcase 1 Success" << "\n";
}


/**
 * \ingroup testing-example
 * This is an example TestCase.
 */
class QuaternionTestCase2 : public TestCase
{
public:
  /** Constructor. */
  QuaternionTestCase2 ();
  /** Destructor. */
  virtual ~QuaternionTestCase2 ();

private:
  virtual void DoRun (void);
};

/** Add some help text to this case to describe what it is intended to test. */
QuaternionTestCase2::QuaternionTestCase2 ()
  : TestCase ("Quaternion test case 2")
{}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
QuaternionTestCase2::~QuaternionTestCase2 ()
{}

void
QuaternionTestCase2::DoRun (void)
{
  Vector V = Vector(0, 1, 1);
  double Vl = V.GetLength();
  V.x /= Vl;
  V.y /= Vl;
  V.z /= Vl;
  Quaternion Q = Quaternion(M_PI * 0.25, V);
  Q.normalize();
  std::cout << "Testcase 2" << "\n";
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.GetLength(), 1.0, 0.001, "Not equal within tolerance");
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.angle(), M_PI * 0.25, 0.001, "Not equal within tolerance"); 
  std::cout << "Testcase 2 Success" << "\n";
}


/**
 * \ingroup testing-example
 * This is an example TestCase.
 */
class QuaternionTestCase3 : public TestCase
{
public:
  /** Constructor. */
  QuaternionTestCase3 ();
  /** Destructor. */
  virtual ~QuaternionTestCase3 ();

private:
  virtual void DoRun (void);
};

/** Add some help text to this case to describe what it is intended to test. */
QuaternionTestCase3::QuaternionTestCase3 ()
  : TestCase ("Quaternion test case 3")
{}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
QuaternionTestCase3::~QuaternionTestCase3 ()
{}

void
QuaternionTestCase3::DoRun (void)
{
  Vector V = Vector(1, 2, 3);
  double Vl = V.GetLength();
  V.x /= Vl;
  V.y /= Vl;
  V.z /= Vl;
  Quaternion Q = Quaternion(M_PI * 0.25, V);
  Q.normalize();
  std::cout << "Testcase 3" << "\n";
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.GetLength(), 1.0, 0.001, "Not equal within tolerance");
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.angle(), M_PI * 0.25, 0.001, "Not equal within tolerance");
  std::cout << "Testcase 3 Success" << "\n";
}

/**
 * \ingroup testing-example
 * This is an example TestCase.
 */
class QuaternionTestCase4 : public TestCase
{
public:
  /** Constructor. */
  QuaternionTestCase4 ();
  /** Destructor. */
  virtual ~QuaternionTestCase4 ();

private:
  virtual void DoRun (void);
};

/** Add some help text to this case to describe what it is intended to test. */
QuaternionTestCase4::QuaternionTestCase4 ()
  : TestCase ("Quaternion: test equivalence between eulerAngles and yaw, pitch, roll")
{}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
QuaternionTestCase4::~QuaternionTestCase4 ()
{}

void
QuaternionTestCase4::DoRun (void)
{
  Quaternion Q = Quaternion(0, 0, 1, 1);
  std::cout << "Testcase 4" << "\n";
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.roll(), Q.eulerAngles().x, 0.001, "Not equal within tolerance"); 
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.pitch(), Q.eulerAngles().y, 0.001, "Not equal within tolerance"); 
  NS_TEST_ASSERT_MSG_EQ_TOL (Q.yaw(), Q.eulerAngles().z, 0.001, "Not equal within tolerance");
  std::cout << "Testcase 4 Success" << "\n";
}

/**
 * \ingroup testing-example
 * This is an example TestCase.
 */
class QuaternionTestCase5 : public TestCase
{
public:
  /** Constructor. */
  QuaternionTestCase5 ();
  /** Destructor. */
  virtual ~QuaternionTestCase5 ();

private:
  virtual void DoRun (void);
};

/** Add some help text to this case to describe what it is intended to test. */
QuaternionTestCase5::QuaternionTestCase5 ()
  : TestCase ("Quaternion: test equivalence between eulerAngles and yaw, pitch, roll")
{}

/**
 * This destructor does nothing but we include it as a reminder that
 * the test case should clean up after itself
 */
QuaternionTestCase5::~QuaternionTestCase5 ()
{}

void
QuaternionTestCase5::DoRun (void)
{
  std::cout << "Testcase 5" << "\n";
  std::vector<std::tuple<double, Vector, Vector>> data = {
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(1,0,0), Vector(1,0,0)),
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(0,1,0), Vector(0,0,-1)),
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(0,0,1), Vector(0,1,0)),
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(-1,0,0), Vector(1,0,0)),
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(0,-1,0), Vector(0,0,1)),
      std::make_tuple<double, Vector, Vector>(M_PI / 2.0, Vector(0,0,-1), Vector(0,-1,0)),
  };

  for (const auto & tuple: data) {
      Quaternion Q = Quaternion(std::get<0>(tuple), std::get<1>(tuple));
      Q.normalize();
      Vector R = Q.rotate(Vector(1, 0, 0));
      NS_TEST_ASSERT_MSG_EQ_TOL (R.x, std::get<2>(tuple).x, 0.001, "Not equal within tolerance"); 
      NS_TEST_ASSERT_MSG_EQ_TOL (R.y, std::get<2>(tuple).y, 0.001, "Not equal within tolerance"); 
      NS_TEST_ASSERT_MSG_EQ_TOL (R.z, std::get<2>(tuple).z, 0.001, "Not equal within tolerance"); 
    }
  std::cout << "Testcase 5 Success" << "\n";
}

/**
 * \ingroup testing-example
 * The TestSuite class names the TestSuite, identifies what type of TestSuite,
 * and enables the TestCases to be run.  Typically, only the constructor for
 * this class must be defined
 */
class QuaternionTestSuite : public TestSuite
{
public:
  /** Constructor. */
  QuaternionTestSuite ();
};

QuaternionTestSuite::QuaternionTestSuite ()
  : TestSuite ("quaternion")
{
  AddTestCase (new QuaternionTestCase1);
  AddTestCase (new QuaternionTestCase2);
  AddTestCase (new QuaternionTestCase3);
  AddTestCase (new QuaternionTestCase4);
  AddTestCase (new QuaternionTestCase5);
}

// Do not forget to allocate an instance of this TestSuite
/**
 * \ingroup testing-example
 * QuaternionTestSuite instance variable.
 */
static QuaternionTestSuite g_sampleTestSuite;


}    // namespace tests

}  // namespace ns3
